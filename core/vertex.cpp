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
  OSAP::debug("unfinished pingHandler at " + name);
  stackClearSlot(item);
}

void Vertex::scopeRequestHandler(stackItem* item, uint16_t ptr){
  OSAP::debug("unfinished scopeHandler at " + name);
  stackClearSlot(item);
}


void Vertex::onOriginStackClear(uint8_t slot){
  if(onOriginStackClear_cb != nullptr) return onOriginStackClear_cb(this, slot);
}

void Vertex::onDestinationStackClear(uint8_t slot){
  if(onDestinationStackClear_cb != nullptr) return onDestinationStackClear_cb(this, slot);
}

// ---------------------------------------------- VPort Constructor and Defaults 

VPort::VPort(
  Vertex* _parent, String _name,
  void (*_loop)(Vertex* vt),
  void (*_send)(VPort* vp, uint8_t* data, uint16_t len),
  boolean (*_cts)(VPort* vp),
  void (*_onOriginStackClear)(Vertex* vt, uint8_t slot),
  void (*_onDestinationStackClear)(Vertex* vt, uint8_t slot)
) : Vertex(_parent, "vp_" + _name, _loop, _onOriginStackClear, _onDestinationStackClear) {
  // set type, reacharound, & callbacks 
  type = VT_TYPE_VPORT;
  vport = this; 
  // set callbacks, 
  send_cb = _send;
  cts_cb = _cts;
}

void VPort::send(uint8_t* data, uint16_t len){
  if(send_cb) return send_cb(this, data, len);
}

boolean VPort::cts(void){
  if(cts_cb) return cts_cb(this);
  return true;
}

// ---------------------------------------------- VBus Constructor and Defaults 

VBus::VBus(
  Vertex* _parent, String _name,
  void (*_loop)(Vertex* vt),
  void (*_send)(VBus* vb, uint8_t* data, uint16_t len, uint8_t rxAddr),
  boolean (*_cts)(VBus* vb, uint8_t rxAddr),
  void (*_broadcast)(VBus* vb, uint8_t* data, uint16_t len, uint8_t broadcastChannel),
  boolean (*_ctb)(VBus* vb, uint8_t broadcastChannel),
  void (*_onOriginStackClear)(Vertex* vt, uint8_t slot),
  void (*_onDestinationStackClear)(Vertex* vt, uint8_t slot)
) : Vertex(_parent, "vb_" + _name, _loop, _onOriginStackClear, _onDestinationStackClear) {
  // set type, reacharound, & callbacks 
  type = VT_TYPE_VBUS;
  vbus = this;
  send_cb = _send;
  cts_cb = _cts;
}

void VBus::send(uint8_t* data, uint16_t len, uint8_t rxAddr){
  if(send_cb) return send_cb(this, data, len, rxAddr);
}

void VBus::broadcast(uint8_t* data, uint16_t len, uint8_t broadcastChannel){
  if(broadcast_cb) return broadcast_cb(this, data, len, broadcastChannel);
}

boolean VBus::cts(uint8_t rxAddr){
  if(cts_cb) return cts_cb(this, rxAddr);
  return true;
}

boolean VBus::ctb(uint8_t broadcastChannel){
  if(ctb_cb) return ctb_cb(this, broadcastChannel);
  return true;
}