/*
osap/vertex.cpp

graph vertex 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vertex.h"
#include "osap.h"
#include "../../../syserror.h"

// ---------------------------------------------- Vertex Constructor and Defaults 

void vtLoopDefault(vertex_t* vt){}
void vtOnOriginStackClearDefault(vertex_t* vt, uint8_t slot){}
void vtOnDestinationStackClearDefault(vertex_t* vt, uint8_t slot){} 

vertex_t::vertex_t(vertex_t* _parent, String _name){
  if(_parent == nullptr){
    type = VT_TYPE_ROOT;
    indice = 0;
  } 
  name = _name;
  stackReset(this);
}

// ---------------------------------------------- VPort Constructor and Defaults 

void vpSendDefault(vport_t* vp, uint8_t* data, uint16_t len){}
boolean vpCtsDefault(vport_t* vp){ return true; }

vport_t::vport_t(
  vertex_t* parent, String _name,
  void (*_loop)(vertex_t* vt),
  void (*_send)(vport_t* vp, uint8_t* data, uint16_t len),
  boolean (*_cts)(vport_t* vp),
  void (*_onOriginStackClear)(vertex_t* vt, uint8_t slot),
  void (*_onDestinationStackClear)(vertex_t* vt, uint8_t slot)
){
  vt.name = "vp_" + _name;
  vt.type = VT_TYPE_VPORT;
  vt.vport = this;
  // add to sys, 
  osapAddVertex(parent, &vt);
  // set callbacks, 
  vt.loop = _loop;
  send = _send;
  cts = _cts;
  if(_onOriginStackClear != nullptr) vt.onOriginStackClear = _onOriginStackClear;
  if(_onDestinationStackClear != nullptr) vt.onDestinationStackClear = _onDestinationStackClear;
}

// ---------------------------------------------- VBus Constructor and Defaults 

void vbSendDefault(vbus_t* vb, uint8_t* data, uint16_t len, uint8_t rxAddr){}
boolean vbCtsDefault(vbus_t* vb, uint8_t rxAddr){ return true; }

vbus_t::vbus_t(
  vertex_t* parent, String _name,
  void (*_loop)(vertex_t* vt),
  void (*_send)(vbus_t* vb, uint8_t* data, uint16_t len, uint8_t rxAddr),
  boolean (*_cts)(vbus_t* vb, uint8_t rxAddr),
  void (*_onOriginStackClear)(vertex_t* vt, uint8_t slot),
  void (*_onDestinationStackClear)(vertex_t* vt, uint8_t slot)
){
  vt.name = "vb_" + _name;
  vt.type = VT_TYPE_VBUS;
  vt.vbus = this;
  // add to sys, 
  osapAddVertex(parent, &vt);
  // set callbacks, 
  vt.loop = _loop;
  send = _send;
  cts = _cts;
  if(_onOriginStackClear != nullptr) vt.onOriginStackClear = _onOriginStackClear;
  if(_onDestinationStackClear != nullptr) vt.onDestinationStackClear = _onDestinationStackClear;
}

// ---------------------------------------------- Stack Tools 

void stackReset(vertex_t* vt){
  // clear all elements & write next ptrs in linear order 
  for(uint8_t od = 0; od < 2; od ++){
    // set lengths, etc, 
    for(uint8_t s = 0; s < vt->stackSize; s ++){
      vt->stack[od][s].arrivalTime = 0;
      vt->stack[od][s].len = 0;
      vt->stack[od][s].indice = s;
    }
    // set next ptrs, 
    for(uint8_t s = 0; s < vt->stackSize - 1; s ++){
      vt->stack[od][s].next = &(vt->stack[od][s + 1]);
    }
    vt->stack[od][vt->stackSize - 1].next = &(vt->stack[od][0]);
    // set previous ptrs, 
    for(uint8_t s = 1; s < vt->stackSize; s ++){
      vt->stack[od][s].previous = &(vt->stack[od][s - 1]);
    }
    vt->stack[od][0].previous = &(vt->stack[od][vt->stackSize - 1]);
    // 1st element is 0th on startup, 
    vt->queueStart[od] = &(vt->stack[od][0]); 
    // first free = tail at init, 
    vt->firstFree[od] = &(vt->stack[od][0]);
  }
}

// -------------------------------------------------------- ORIGIN SIDE 
// true if there's any space in the stack, 
boolean stackEmptySlot(vertex_t* vt, uint8_t od){
  if(od > 1) return false;
  // if 1st free has ptr to next item, not full 
  if(vt->firstFree[od]->next->len != 0){
    return false;
  } else {
    return true;
  }
}

// loads data into stack 
void stackLoadSlot(vertex_t* vt, uint8_t od, uint8_t* data, uint16_t len){
  if(od > 1) return; // bad od, lost data 
  // copy into first free element, 
  memcpy(vt->firstFree[od]->data, data, len);
  vt->firstFree[od]->len = len;
  vt->firstFree[od]->arrivalTime = millis();
  //sysError("load " + String(vt->firstFree[od]->indice) + " " + String(vt->firstFree[od]->arrivalTime));
  // now firstFree is next, 
  vt->firstFree[od] = vt->firstFree[od]->next;
}

// -------------------------------------------------------- EXIT SIDE 
// return count of items occupying stack, and list of ptrs to them, 
uint8_t stackGetItems(vertex_t* vt, uint8_t od, stackItem** items, uint8_t maxItems){
  if(od > 1) return 0;
  // when queueStart == firstFree element, we have nothing for you 
  if(vt->firstFree[od] == vt->queueStart[od]) return 0;
  // starting at queue begin, 
  uint8_t count = 0;
  stackItem* item = vt->queueStart[od];
  for(uint8_t s = 0; s < maxItems; s ++){
    items[s] = item;
    count ++;
    if(item->next->len > 0){
      item = item->next;
    } else {
      return count;
    }
  }
  return count;
}

// clear the item, 
void stackClearSlot(vertex_t* vt, uint8_t od, stackItem* item){
  // item is 0-len, etc 
  item->len = 0;
  // is this
  uint8_t indice = item->indice;
  // if was queueStart, queueStart now at next,
  if(vt->queueStart[od] == item){
    vt->queueStart[od] = item->next;
    // and wouldn't have to do any of the below? 
  } else {
    // pull from chain, now is free of associations, 
    // these ops are *always two up*
    item->previous->next = item->next;
    item->next->previous = item->previous;
    // now, insert this where old firstFree was 
    vt->firstFree[od]->previous->next = item;
    item->previous = vt->firstFree[od]->previous;    
    item->next = vt->firstFree[od];
    vt->firstFree[od]->previous = item;
    // and the item is the new firstFree element, 
    vt->firstFree[od] = item;
  }
  // now we callback to the vertex; these fns are often used to clear flowcontrol condns 
  switch(od){
    case VT_STACK_ORIGIN:
      vt->onOriginStackClear(vt, indice);
      break;
    case VT_STACK_DESTINATION:
      vt->onDestinationStackClear(vt, indice);
      break;
    default:  // pretty unlikely 
      sysError("stack clear slot, od > 1, que?");
      break;
  }
}
