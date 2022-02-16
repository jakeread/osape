/*
osap/vertices/msegEndpoint.cpp

network : software interface for multisegment endpoints 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2022

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "msegEndpoint.h"
#include "../core/osap.h"
#include "../core/packets.h"
#include "../../../indicators.h"
#include "../../../syserror.h"

// -------------------------------------------------------- State 

#define MAX_CONTEXT_MSEGENDPOINTS 64 
msegEndpoint_t* _msegEndpoints[MAX_CONTEXT_MSEGENDPOINTS];
uint8_t _numMsegEndpoints = 0;

// -------------------------------------------------------- Build / Add 

msegEndpoint_t* osapBuildMsegEndpoing(String name, uint8_t* dptr, uint16_t len){
  vertex_t* vt = new vertex_t;
  vt->type = VT_TYPE_CODE;
  vt->name = name;
  stackReset(vt);
  // add to sys,
  if(osapAddVertex(vt) && (_numMsegEndpoints < MAX_CONTEXT_MSEGENDPOINTS)){
    msegEndpoint_t* msep = new msegEndpoint_t;
    msep->vt = vt;
    _msegEndpoints[_numMsegEndpoints] = msep;
    _numMsegEndpoints ++;
  } else {
    delete vt;
    return nullptr;
  }
}

void msegEndpointMainLoop(void){
  unsigned long now = micros();
  for(uint8_t m = 0; m < _numMsegEndpoints; m ++){
    msegEndpointLoop(_msegEndpoints[m], now);
  }
}

void msegEndpointLoop(msegEndpoint_t* msep, unsigned long now){
  stackItem* items[msep->vt->stackSize];
  uint8_t count = stackGetItems(msep->vt, VT_STACK_DESTINATION, items, msep->vt->stackSize);
  // walk 'em and try to handle, 
  for(uint8_t i = 0; i < count; i ++){
    stackItem* item = items[i];
    uint16_t ptr = 0;
    if(!ptrLoop(item->data, &ptr)){
      sysError("endpoint loop bad ptr walk");
      logPacket(item->data, item->len);
      stackClearSlot(msep->vt, VT_STACK_DESTINATION, item);
      continue;
    }
    //#error you're up to this: we want to be tidying and de-cluttering w/ transfers into packet.cpp, vertex.cpp, etc 
  }
}

void msegHandler(msegEndpoint_t* msep, stackItem* item, uint16_t ptr){

}