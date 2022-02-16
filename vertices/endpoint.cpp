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
#include "../core/osap.h"
#include "../core/packets.h"
#include "../../../indicators.h" 	// indicators are circuit specific, should live 3 levels up 
#include "../../../syserror.h" 		// syserror is also circuit specific, ibid 

// -------------------------------------------------------- Endpoint State

#define MAX_CONTEXT_ENDPOINTS 64

// list of endpoints, and count:
endpoint_t* _endpoints[MAX_CONTEXT_ENDPOINTS];
uint8_t _numEndpoints = 0;

// -------------------------------------------------------- Constructors 

// base constructor, 
endpoint_t::endpoint_t( String _name, 
            EP_ONDATA_RESPONSES (*_onData)(uint8_t* data, uint16_t len),
            boolean (*_beforeQuery)(void)){
  // set name, type, 
  name = _name;
  vt.name = "ep_" + _name;
  vt.type = VT_TYPE_ENDPOINT;
  // and can we add it to osap ? this also inits the vt 
  //osapAddVertex(&vt);
  // set callbacks,
  (_onData == nullptr) ? onData = onDataDefault : onData = _onData;
  (_beforeQuery == nullptr) ? beforeQuery = beforeQueryDefault : beforeQuery = _beforeQuery;
  // add us to list ? 
  _endpoints[_numEndpoints ++] = this;
}

// -------------------------------------------------------- Dummies 

EP_ONDATA_RESPONSES onDataDefault(uint8_t* data, uint16_t len){
  return EP_ONDATA_ACCEPT;
}

boolean beforeQueryDefault(void){
  return true;
}

// -------------------------------------------------------- Endpoint Route / Write API 

void endpointWrite(endpoint_t* ep, uint8_t* data, uint16_t len){
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

// add a route to an endpoint 
boolean endpointAddRoute(endpoint_t* ep, uint8_t* path, uint16_t pathLen, EP_ROUTE_MODES mode){
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

boolean endpointAllClear(endpoint_t* ep){
  for(uint8_t r = 0; r < ep->numRoutes; r ++){
    if(ep->routes[r].state != EP_TX_IDLE){
      return false;
    }
  }
  return true;
}

// -------------------------------------------------------- Runtimes 

void endpointMainLoop(void){
  unsigned long now = micros();
  for(uint8_t e = 0; e < _numEndpoints; e ++){
    endpointLoop(_endpoints[e], now);
  }
}

// temp packet write 
uint8_t ack[VT_SLOTSIZE];
uint8_t EPOut[VT_SLOTSIZE];

// main endpoint loop, 
void endpointLoop(endpoint_t* ep, unsigned long now){
	// ------------------------------------------ RX Check
	// we run a similar loop as the main switch... 
	// though we are only concerned with handling items in the destination stack 
	// so, start by collecting items,
	stackItem* items[ep->vt.stackSize];
	uint8_t count = stackGetItems(&(ep->vt), VT_STACK_DESTINATION, items, ep->vt.stackSize);
	// now we check through 'em and try to handle 
	for(uint8_t i = 0; i < count; i ++){
		stackItem* item = items[i];
		uint16_t ptr = 0;
    // find the ptr, 
    if(!ptrLoop(item->data, &ptr)){
      sysError("endpoint loop bad ptr walk");
      logPacket(item->data, item->len);
      stackClearSlot(&(ep->vt), VT_STACK_DESTINATION, item);
      continue;
    }
    // only continue if pckt is for us
    ptr ++;
    if(item->data[ptr] != PK_DEST) continue;
    // run the switch;
    EP_ONDATA_RESPONSES resp = endpointHandler(ep, item, ptr);
    switch(resp){
      case EP_ONDATA_REJECT:
      case EP_ONDATA_ACCEPT:
        // in either case, we are done and can clear it 
        stackClearSlot(&(ep->vt), VT_STACK_DESTINATION, item);
        break;
      case EP_ONDATA_WAIT:
        // it wants to hang in the stack, so we update the stale time 
        item->arrivalTime = now;
        break;
      default:
        // badness from the handler, doesn't make much sense but 
        sysError("on endpoint dest. handle, unknown ondata response");
        stackClearSlot(&(ep->vt), VT_STACK_DESTINATION, item);
        break;
    } // end dest switch 
	}

	// ------------------------------------------ TX Check 
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
				if(stackEmptySlot(&(ep->vt), VT_STACK_ORIGIN)){
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
					stackLoadSlot(&(ep->vt), VT_STACK_ORIGIN, EPOut, wptr);
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

// item->data[ptr] == PK_DEST here 
// item->data[ptr + 1, ptr + 2] = segsize !
EP_ONDATA_RESPONSES endpointHandler(endpoint_t* ep, stackItem* item, uint16_t ptr){
	ptr += 3;
	switch(item->data[ptr]){
		case EP_SS_ACKLESS: // ah right, these were 'single segment' msgs... yikes 
			{ // endpoints can REJECT, ACCEPT or ask for a WAIT for new data... 
				// with an ackless write, WAIT is risky... but here we are 
				// NOTE: previous code had *ackless* using 'ptr + 1' offsets, but should be the same 
				// as *acked* code, which uses 'ptr + 2' offset ... swapped back here 2021-07-07, ? 
				// was rarely using ackless... so I presume this is it 
				EP_ONDATA_RESPONSES resp = ep->onData(&(item->data[ptr + 2]), item->len - ptr - 2);
				switch(resp){
					case EP_ONDATA_REJECT: // nothing to do: msg will be deleted one level up 
						break; 
					case EP_ONDATA_ACCEPT: // data OK, copy it in:
						memcpy(ep->data, &(item->data[ptr + 2]), item->len - ptr - 2);
						ep->dataLen = item->len - ptr - 2;
						break;
					case EP_ONDATA_WAIT: // nothing to do, msg will be awaited one level up 
						break;
				} // end resp switch 
				return resp; // return the response one level up... 
			} // end ackless case closure 
			break;
		case EP_SS_ACKED:
			{ // if there's not any space for an ack, we won't be able to ack, ask to wait 
				if(!stackEmptySlot(&(ep->vt), VT_STACK_ORIGIN)) return EP_ONDATA_WAIT;
				// otherwise carry on to the handler, 
				EP_ONDATA_RESPONSES resp = ep->onData(&(item->data[ptr + 2]), item->len - ptr - 2);
				switch(resp){
					case EP_ONDATA_ACCEPT:
						// this means we copy the data in, it's the new endpoint data:
						memcpy(ep->data, &(item->data[ptr + 2]), item->len - ptr - 2);
						ep->dataLen = item->len - ptr - 2;
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
              #warning acks can write in place, my dude 
							ack[wptr ++] = EP_SS_ACK; // the ack ID is here in prv packet 
							ack[wptr ++] = item->data[ptr + 1];
							stackLoadSlot(&(ep->vt), VT_STACK_ORIGIN, ack, wptr);
						}
						break; // end accept / reject 
					case EP_ONDATA_WAIT: // again: will mirror this reponse up, wait will happen there 
						break;
					default:
						break;
				}
				return resp; // mirror response to osapLoop.cpp 
			} // end acked case closure 
			break;
		case EP_QUERY:
			// if can generate new message, 
			if(stackEmptySlot(&(ep->vt), VT_STACK_ORIGIN)){
				// run the 'beforeQuery' call, which doesn't need to return anything: 
				ep->beforeQuery();
				uint16_t wptr = 0;
				// if the route can't be reversed, trouble:
				if(!reverseRoute(item->data, ptr - 4, ack, &wptr)) {
					sysError("on a query, can't reverse a route, rming msg");
					return EP_ONDATA_REJECT;
				} else {
					ack[wptr ++] = EP_QUERY_RESP;		// reply is response 
					ack[wptr ++] = item->data[ptr + 1];	// has ID matched to request 
					memcpy(&(ack[wptr]), ep->data, ep->dataLen);
					wptr += ep->dataLen;
					stackLoadSlot(&(ep->vt), VT_STACK_ORIGIN, ack, wptr);
					return EP_ONDATA_ACCEPT;
				}
			} else {
				// no space to ack w/ a query, pls wait 
				return EP_ONDATA_WAIT;
			}
		case EP_SS_ACK:
      { // upd8 tx state on associated route 
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