/*
osap/endpoint.cpp

network : software interface

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "endpoint.h"
#include "osapUtils.h"
#include "../../../indicators.h" 	// indicators are circuit specific, should live 3 levels up 
#include "../../../syserror.h" 		// syserror is also circuit specific, ibid 

uint8_t ack[VT_SLOTSIZE];

// handle, return true to clear out. false to wait one turn 
// item->data[ptr] == PK_DEST here 
// item->data[ptr + 1, ptr + 2] = segsize !
EP_ONDATA_RESPONSES endpointHandler(vertex_t* vt, uint8_t od, stackItem* item, uint16_t ptr){
	ptr += 3;
	switch(item->data[ptr]){
		case EP_SS_ACKLESS: // ah right, these were 'single segment' msgs... yikes 
			{
				// endpoints can REJECT, ACCEPT or ask for a WAIT for new data... 
				// with an ackless write, WAIT is risky... but here we are 
				// NOTE: previous code had *ackless* using 'ptr + 1' offsets, but should be the same 
				// as *acked* code, which uses 'ptr + 2' offset ... swapped back here 2021-07-07, ? 
				// was rarely using ackless... so I presume this is it 
				EP_ONDATA_RESPONSES resp = vt->ep->onData(&(item->data[ptr + 2]), item->len - ptr - 2);
				switch(resp){
					case EP_ONDATA_REJECT:
						// nothing to do: msg will be deleted one level up 
						break; 
					case EP_ONDATA_ACCEPT:
						// data OK, copy it in:
						memcpy(vt->ep->data, &(item->data[ptr + 2]), item->len - ptr - 2);
						vt->ep->dataLen = item->len - ptr - 2;
						break;
					case EP_ONDATA_WAIT:
						// nothing to do, msg will be awaited one level up 
						break;
				} // end resp switch 
				return resp; // return the response one level up... 
			} // end ackless case closure 
			break;
		case EP_SS_ACKED:
			{
				// if there's not any space for an ack, we won't be able to ack, ask to wait 
				if(!stackEmptySlot(vt, VT_STACK_ORIGIN)) return EP_ONDATA_WAIT;
				// otherwise carry on to the handler, 
				EP_ONDATA_RESPONSES resp = vt->ep->onData(&(item->data[ptr + 2]), item->len - ptr - 2);
				switch(resp){
					case EP_ONDATA_WAIT:
						// again: will mirror this reponse up, wait will happen there 
						break;
					case EP_ONDATA_ACCEPT:
						// this means we copy the data in, it's the new endpoint data:
						memcpy(vt->ep->data, &(item->data[ptr + 2]), item->len - ptr - 2);
						vt->ep->dataLen = item->len - ptr - 2;
						// carry on to generate the response (no break)
					case EP_ONDATA_REJECT:
						{
							// for reject *or* accept, we acknowledge that we got the data: 
							// now generate are reply, 
							uint16_t wptr = 0;
							if(!reverseRoute(item->data, ptr - 4, ack, &wptr)){
								// if we can't reverse it, bail, but issue same response to 
								// osapLoop.cpp
								return resp; 
							}
							// the ack, 
							ack[wptr ++] = EP_SS_ACK;
              // the ack ID is here in prv packet 
							ack[wptr ++] = item->data[ptr + 1];
							stackLoadSlot(vt, VT_STACK_ORIGIN, ack, wptr);
						}
						break; // end accept / reject 
					default:
						break;
				}
				return resp; // mirror response to osapLoop.cpp 
			} // end acked case closure 
			break;
		case EP_QUERY:
			// if can generate new message, 
			if(stackEmptySlot(vt, VT_STACK_ORIGIN)){
				// run the 'beforeQuery' call, which doesn't need to return anything: 
				vt->ep->beforeQuery();
				uint16_t wptr = 0;
				// if the route can't be reversed, trouble:
				if(!reverseRoute(item->data, ptr - 4, ack, &wptr)) {
					sysError("on a query, can't reverse a route, rming msg");
					return EP_ONDATA_REJECT;
				} else {
					ack[wptr ++] = EP_QUERY_RESP;		// reply is response 
					ack[wptr ++] = item->data[ptr + 1];	// has ID matched to request 
					memcpy(&(ack[wptr]), vt->ep->data, vt->ep->dataLen);
					wptr += vt->ep->dataLen;
					stackLoadSlot(vt, VT_STACK_ORIGIN, ack, wptr);
					return EP_ONDATA_ACCEPT;
				}
			} else {
				// no space to ack w/ a query, pls wait 
				return EP_ONDATA_WAIT;
			}
		case EP_SS_ACK:
      // upd8 tx state on associated route 
      {
        endpoint_t* ep = vt->ep;
        for(uint8_t r = 0; r < ep->numRoutes; r ++){
          // this is where the ackId is in the packet, we match to routes on that (for speed)
          if(item->data[ptr + 1] == ep->routes[r].ackId){
            switch(ep->routes[r].state){
              case EP_TX_AWAITING_ACK:  // awaiting -> captured -> idle, 
                ep->routes[r].state = EP_TX_IDLE;
                break;
              case EP_TX_AWAITING_AND_FRESH:  // awaiting -> captured -> fresh again 
                ep->routes[r].state = EP_TX_FRESH;
                break;
              case EP_TX_FRESH:
              case EP_TX_IDLE:
              default:
                // these are all nonsense states for receipt of an ack, 
                break;
            }
          }
        }
      }
      return EP_ONDATA_ACCEPT;
		case EP_QUERY_RESP:
			// not yet having embedded query function 
		default:
			// not recognized, bail city, get it outta here,
			return EP_ONDATA_REJECT;
	}
}

// add a route to an endpoint 
boolean addRouteToEndpoint(vertex_t* vt, uint8_t* path, uint16_t pathLen, EP_ROUTE_MODES mode){
	// if vertex is an endpoint, get handle for it 
	if(vt->ep == nullptr) return false; 
	endpoint_t* ep = vt->ep;
	// guard against more-than-allowed routes 
	if(ep->numRoutes >= ENDPOINT_MAX_ROUTES) return false;
	// handle for the route we're going to modify (and increment # of active routes)
	endpoint_route_t* rt = &(ep->routes[ep->numRoutes ++]);
	// load the path -> the path 
	memcpy(rt->path, path, pathLen);
	rt->pathLen = pathLen;
	rt->ackMode = mode;
	// done 
	return true; 
}

void endpointWrite(vertex_t* vt, uint8_t* data, uint16_t len){
  // if vertex is an endpoint, get handle for it 
	if(vt->ep == nullptr) return; 
	endpoint_t* ep = vt->ep;
  // copy data in,
  if(len > VT_SLOTSIZE) return; // no lol 
  memcpy(ep->data, data, len);
  ep->dataLen = len;
  // set route freshness 
  for(uint8_t r = 0; r < ep->numRoutes; r ++){
    if(ep->routes[r].state == EP_TX_AWAITING_ACK){
      ep->routes[r].state = EP_TX_AWAITING_AND_FRESH;
    } else {
      ep->routes[r].state = EP_TX_FRESH;
    }
  }
}

boolean endpointAllClear(endpoint_t* ep){
  for(uint8_t r = 0; r < ep->numRoutes; r ++){
    if(ep->routes[r].state != EP_TX_IDLE){
      return false;
    }
  }
  return true;
}

uint8_t EPOut[VT_SLOTSIZE];

// check tx states, 
void endpointLoop(endpoint_t* ep, unsigned long now){
	// we want to pluck 'em out on round-robin...
	uint8_t r = ep->lastRouteServiced;
	for(uint8_t i = 0; i < ep->numRoutes; i ++){
		r ++;
		if(r >= ep->numRoutes) r = 0;
		endpoint_route_t* rt = &(ep->routes[r]);
		switch(rt->state){
			case EP_TX_IDLE:
				// no-op 
				break;
			case EP_TX_FRESH:
				// foruml8 pck & tx it 
				if(stackEmptySlot(ep->vt, VT_STACK_ORIGIN)){
					// load it w/ data, 
					#warning slow-load code, should write str8 to stack 
					// write ptr in the head, 
					uint16_t wptr = 0;
					EPOut[wptr ++] = PK_PTR;
					// the path next, 
					memcpy(&(EPOut[wptr]), rt->path, rt->pathLen);
					wptr += rt->pathLen;
					// destination key, segment size 
					EPOut[wptr ++] = PK_DEST;
					ts_writeUint16(rt->segSize, EPOut, &wptr);
					// mode-key, 
					if(rt->ackMode == EP_ROUTE_ACKLESS){
            EPOut[wptr ++] = EP_SS_ACKLESS;
					} else if(rt->ackMode == EP_ROUTE_ACKED){
            EPOut[wptr ++] = EP_SS_ACKED;
            EPOut[wptr ++] = ep->nextAckId;
            rt->ackId = ep->nextAckId;
            ep->nextAckId ++; // increment and wrap: only one ID per endpoint per tx, for demux 
          }
					// check against write into stray memory 
					if(ep->dataLen + wptr >= VT_SLOTSIZE){
						ERROR(1, "write-to-endpoint exceeds slotsize");
						return;
					}
					// the data, 
					memcpy(&(EPOut[wptr]), ep->data, ep->dataLen);
					wptr += ep->dataLen;
					// that's a packet? we load it into stack, we're done 
          rt->txTime = now;
					stackLoadSlot(ep->vt, VT_STACK_ORIGIN, EPOut, wptr);
					// transition state:
					if(rt->ackMode == EP_ROUTE_ACKLESS) rt->state = EP_TX_IDLE;
					if(rt->ackMode == EP_ROUTE_ACKED) rt->state = EP_TX_AWAITING_ACK;
					// and track, so that we do *this recently serviced* thing *last* on next round 
					ep->lastRouteServiced = r;
				} else {
					// no space... await... 
				}
			case EP_TX_AWAITING_ACK:
				// check timeout & transition to idle state 
        if(rt->txTime + rt->timeoutLength > now){
          rt->state = EP_TX_IDLE;
        }
				break;
      case EP_TX_AWAITING_AND_FRESH:
        // check timeout & transition to fresh state 
        if(rt->txTime + rt->timeoutLength > now){
          rt->state = EP_TX_FRESH;
        }
        break;
			default:
				break;
		}
	}
}
