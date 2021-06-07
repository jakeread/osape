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
boolean endpointHandler(vertex_t* vt, uint8_t od, stackItem* item, uint16_t ptr){
	ptr += 3;
	switch(item->data[ptr]){
		case EP_SS_ACKLESS:
			if(vt->ep->onData(&(item->data[ptr + 1]), item->len - ptr - 1)){
				// data accepted... copy in to local store & clear 
				memcpy(vt->ep->data, &(item->data[ptr + 1]), item->len - ptr - 1);
				vt->ep->dataLen = item->len - ptr - 1;
				return true;
			} else {
				return false;
			}
			break;
		case EP_SS_ACKED:
			// check if we can ack it, then if we can accept it: 
			if(stackEmptySlot(vt, VT_STACK_ORIGIN)){
				if(vt->ep->onData(&(item->data[ptr + 2]), item->len - ptr - 2)){
					// accepted, we can copy in 
					memcpy(vt->ep->data, &(item->data[ptr + 2]), item->len - ptr - 2);
					vt->ep->dataLen = item->len - ptr - 2;
					// now generate are reply, 
					uint16_t wptr = 0;
					if(!reverseRoute(item->data, ptr - 4, ack, &wptr)) return true; // if we can't reverse it, bail 
					// the ack, 
					ack[wptr ++] = EP_SS_ACK;
					ack[wptr ++] = item->data[ptr + 1];
					stackLoadSlot(vt, VT_STACK_ORIGIN, ack, wptr);
					return true;
				} else {	// data not accepted at endpoint, wait 
					return false;
				}
			} else { // no available ack space, await: 
				return false; 
			}
			break;
		case EP_QUERY:
			// if can generate new message, 
			if(stackEmptySlot(vt, VT_STACK_ORIGIN)){
				vt->ep->beforeQuery();
				uint16_t wptr = 0;
				if(!reverseRoute(item->data, ptr - 4, ack, &wptr)) return true;
				ack[wptr ++] = EP_QUERY_RESP;
				ack[wptr ++] = item->data[ptr + 1];
				memcpy(&(ack[wptr]), vt->ep->data, vt->ep->dataLen);
				wptr += vt->ep->dataLen;
				stackLoadSlot(vt, VT_STACK_ORIGIN, ack, wptr);
			}
		case EP_SS_ACK:
		case EP_QUERY_RESP:
		default:
			// not yet handling embedded endpoints as data sources ... nor having embedded query function 
			// not recognized, bail city, get it outta here,
			return true;
	}
}