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
						// carry on to generate the response 
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
		case EP_QUERY_RESP:
		default:
			// not yet handling embedded endpoints as data sources ... nor having embedded query function 
			// not recognized, bail city, get it outta here,
			return EP_ONDATA_REJECT;
	}
}