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
#include "stack.h"
#include "osap.h"
#include "packets.h"

// ---------------------------------------------- Temporary Stash 

uint8_t Vertex::payload[VT_SLOTSIZE];
uint8_t Vertex::datagram[VT_SLOTSIZE];

// ---------------------------------------------- Vertex Constructor and Defaults 

Vertex::Vertex( 
  Vertex* _parent, String _name, 
  void (*_loop)(Vertex* vt),
  void (*_onOriginStackClear)(Vertex* vt, uint8_t slot),
  void (*_onDestinationStackClear)(Vertex* vt, uint8_t slot)
){
  // name self, reset stack... 
  name = _name;
  stackReset(this);
  // callback assignments... 
  loop_cb = _loop;
  onOriginStackClear_cb = _onOriginStackClear;
  onDestinationStackClear_cb = _onDestinationStackClear;
  // insert self to osap net,
  if(_parent == nullptr){
    type = VT_TYPE_ROOT;
    indice = 0;
  } else {
    if (_parent->numChildren >= VT_MAXCHILDREN) {
      OSAP::error("trying to nest a vertex under " + _parent->name + " but we have reached VT_MAXCHILDREN limit", HALTING);
    } else {
      this->indice = _parent->numChildren;
      this->parent = _parent;
      _parent->children[_parent->numChildren ++] = this;
    }
  }
}

void Vertex::loop(void){
  if(loop_cb != nullptr) return loop_cb(this);
}

void Vertex::destHandler(stackItem* item, uint16_t ptr){
  // generic handler...
  OSAP::debug("generic destHandler at " + name);
  stackClearSlot(item);
}

void Vertex::pingRequestHandler(stackItem* item, uint16_t ptr){
  // key & id, 
  payload[0] = PK_PINGRES;
  payload[1] = item->data[ptr + 2];
  // write a new gram, 
  uint16_t len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, 2);
  // clear previous, 
  stackClearSlot(item);
  // load next... there will be one empty, as this has just arrived here... & we just wiped it 
  stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
}

void Vertex::scopeRequestHandler(stackItem* item, uint16_t ptr){
  // key & id, 
  payload[0] = PK_SCOPERES;
  payload[1] = item->data[ptr + 2];
  // next items write starting here, 
  uint16_t wptr = 2;
  // scope time-tag, 
  ts_writeUint32(scopeTimeTag, payload, &wptr);
  // and read in the previous scope (this is traversal state required to delineate loops in the graph) 
  uint16_t rptr = ptr + 3;
  ts_readUint32(&scopeTimeTag, item->data, &rptr);
  // write the vertex type,  
  payload[wptr ++] = type;
  // vport / vbus link states, 
  if(type == VT_TYPE_VPORT){
    payload[wptr ++] = (vport->isOpen() ? 1 : 0);
  } else if (type == VT_TYPE_VBUS){
    uint16_t addrSize = vbus->addrSpaceSize;
    uint16_t addr = 0;
    // ok we write the address size in first, 
    ts_writeUint16(vbus->addrSpaceSize, payload, &wptr);
    // then *so long a we're not overwriting*, we stuff link-state bytes, 
    while(wptr + 6 + name.length() <= VT_SLOTSIZE){
      payload[wptr] = 0;
      for(uint8_t b = 0; b < 8; b ++){
        payload[wptr] |= (vbus->isOpen(addr) ? 1 : 0) << b;
        addr ++;
        if(addr >= addrSize) goto end;
      }
      wptr ++;
    }
    end:
    wptr ++; // += 1 more, so we write into next, 
  }
  // our own indice, # siblings, and # children, 
  ts_writeUint16(indice, payload, &wptr);
  if(parent != nullptr){
    ts_writeUint16(parent->numChildren, payload, &wptr);
  } else {
    ts_writeUint16(0, payload, &wptr);
  }
  ts_writeUint16(numChildren, payload, &wptr);
  // finally, our string name:
  ts_writeString(name, payload, &wptr);
  // and roll that back up, rm old, and ship it, 
  uint16_t len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, wptr);
  stackClearSlot(item);
  stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
}


void Vertex::onOriginStackClear(uint8_t slot){
  if(onOriginStackClear_cb != nullptr) return onOriginStackClear_cb(this, slot);
}

void Vertex::onDestinationStackClear(uint8_t slot){
  if(onDestinationStackClear_cb != nullptr) return onDestinationStackClear_cb(this, slot);
}

// ---------------------------------------------- VPort Constructor and Defaults 

VPort::VPort(
  Vertex* _parent, String _name
) : Vertex(_parent, "vp_" + _name, nullptr, nullptr, nullptr) {
  // set type, reacharound, & callbacks 
  type = VT_TYPE_VPORT;
  vport = this; 
}

// ---------------------------------------------- VBus Constructor and Defaults 

VBus::VBus(
  Vertex* _parent, String _name
) : Vertex(_parent, "vb_" + _name, nullptr, nullptr, nullptr) {
  // set type, reacharound, & callbacks 
  type = VT_TYPE_VBUS;
  vbus = this;
}
