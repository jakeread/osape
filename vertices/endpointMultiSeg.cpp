/*
osap/vertices/endpointMultiSeg.cpp

network : software interface for big bois 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2022

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "endpointMultiSeg.h"
#include "../core/osap.h"
#include "../core/packets.h"
#ifdef OSAP_DEBUG
#include "./osap_debug.h"
#endif 

// -------------------------------------------------------- Constructors 

EndpointMultiSeg::EndpointMultiSeg(
  Vertex* _parent, String _name,
  void* _dataPtr, uint32_t _dataLen
) : Vertex(_parent, "epmseg_" + _name){
  type = VT_TYPE_ENDPOINT_MULTISEG;
  // no callbacks, just 
  dataPtr = _dataPtr;
  dataLen = _dataLen;
}

// -------------------------------------------------------- Das Loop 

uint8_t tempResp[256];
uint16_t tempWptr = 0;

void EndpointMultiSeg::loop(void){
  // get msgs from incoming stack, 
  stackItem* items[stackSize];
  uint8_t count = stackGetItems(this, VT_STACK_DESTINATION, items, stackSize);
  // try handle, 
  for(uint8_t i = 0; i < count; i ++){
    stackItem* item = items[i];
    uint16_t ptr = 0;
    // find ptr,
    if(!findPtr(item->data, &ptr)){
      ERROR(1, "mseg endpoint bad ptr loop walk");
      stackClearSlot(this, VT_STACK_DESTINATION, item);
      continue;
    } 
    // to reply, we need to have empty space, 
    // note: not true if we can write-back directly into destination stack, 
    // but we want to open incoming spaces... 
    if (!stackEmptySlot(this, VT_STACK_ORIGIN)) continue;
    // pckt for us?
    ptr ++;
    if(item->data[ptr] != PK_DEST) continue;
    // item->data[ptr] == PK_DEST, so 
    // item->data[ptr + 1, ptr + 2] == segsize, so we do:
    ptr += 3;
    // now we have 1st byte of 'application' layer 
    switch(item->data[ptr]){
      case EPMSEG_QUERY: {  
          // query case, should be [route][ptr][segsize:2][dest:1][query:1][startread:2][endread:2]
          // collect start, end to read back, 
          ptr ++;
          uint16_t start, end; 
          ts_readUint16(&start, item->data, &ptr); ts_readUint16(&end, item->data, &ptr);
          // reverse the route out, writing into our temporary pckt, 
          tempWptr = 0;
          if(!reverseRoute(item->data, ptr - 9, tempResp, &tempWptr)){
            ERROR(1, "reverse route fails on epmseg query");
          } else {  // reverse route OK, stuff pckt, 
            // find end... 
            if(end >= dataLen) end = dataLen;
            if(start < end && end <= dataLen && end - start + tempWptr < VT_SLOTSIZE){
              if(end == dataLen){
                tempResp[tempWptr ++] = EPMSEG_QUERY_END_RESP;
              } else {
                tempResp[tempWptr ++] = EPMSEG_QUERY_RESP;
              }
              ts_writeUint16(start, tempResp, &tempWptr);
              ts_writeUint16(end, tempResp, &tempWptr);
              // memcpy from / to 
              memcpy(&(tempResp[tempWptr]), dataPtr + start, end - start);
              // that's a packet innit, 
              stackLoadSlot(this, VT_STACK_ORIGIN, tempResp, tempWptr + end - start);
            } else {
              ERROR(2, "mseg query too long or ptrs backwards");
            }
          }
          // no wait terms, so we can always clear this
          stackClearSlot(this, VT_STACK_DESTINATION, item);
        }
        break;
      default:
        ERROR(1, "mseg endpoint unrecognized dest switch" + String(item->data[ptr]));
        stackClearSlot(this, VT_STACK_DESTINATION, item);
        break;
    }
  }
}