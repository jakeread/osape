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
#ifdef OSAP_DEBUG
#include "./osap_debug.h"
#endif 

// ---------------------------------------------- Vertex Constructor and Defaults 

void vtLoopDefault(Vertex* vt){}
void vtOnOriginStackClearDefault(Vertex* vt, uint8_t slot){}
void vtOnDestinationStackClearDefault(Vertex* vt, uint8_t slot){} 

Vertex::Vertex(Vertex* _parent, String _name){
  if(_parent == nullptr){
    type = VT_TYPE_ROOT;
    indice = 0;
  } else {
    osapAddVertex(_parent, this);
  }
  name = _name;
  stackReset(this);
}

// ---------------------------------------------- VPort Constructor and Defaults 

void vpSendDefault(VPort* vp, uint8_t* data, uint16_t len){}
boolean vpCtsDefault(VPort* vp){ return true; }

VPort::VPort(
  Vertex* _parent, String _name,
  void (*_loop)(Vertex* vt),
  void (*_send)(VPort* vp, uint8_t* data, uint16_t len),
  boolean (*_cts)(VPort* vp),
  void (*_onOriginStackClear)(Vertex* vt, uint8_t slot),
  void (*_onDestinationStackClear)(Vertex* vt, uint8_t slot)
) : Vertex(_parent, "vp_" + _name) {
  // lol, so here we are setting it such that we can later do vt->vp... idk 
  type = VT_TYPE_VPORT;
  vport = this; 
  loop = _loop;
  // set callbacks, 
  send = _send;
  if(_cts != nullptr) cts = _cts;
  if(_onOriginStackClear != nullptr) onOriginStackClear = _onOriginStackClear;
  if(_onDestinationStackClear != nullptr) onDestinationStackClear = _onDestinationStackClear;
}

// ---------------------------------------------- VBus Constructor and Defaults 

void vbSendDefault(VBus* vb, uint8_t* data, uint16_t len, uint8_t rxAddr){}
boolean vbCtsDefault(VBus* vb, uint8_t rxAddr){ return true; }

VBus::VBus(
  Vertex* _parent, String _name,
  void (*_loop)(Vertex* vt),
  void (*_send)(VBus* vb, uint8_t* data, uint16_t len, uint8_t rxAddr),
  boolean (*_cts)(VBus* vb, uint8_t rxAddr),
  void (*_onOriginStackClear)(Vertex* vt, uint8_t slot),
  void (*_onDestinationStackClear)(Vertex* vt, uint8_t slot)
) : Vertex(_parent, "vb_" + _name) {
  type = VT_TYPE_VBUS;
  vbus = this;
  loop = _loop;
  send = _send;
  cts = _cts;
  if(_onOriginStackClear != nullptr) onOriginStackClear = _onOriginStackClear;
  if(_onDestinationStackClear != nullptr) onDestinationStackClear = _onDestinationStackClear;
}
