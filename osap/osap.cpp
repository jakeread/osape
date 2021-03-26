/*
osap/osap.cpp

osap root / vertex factory

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#include "osap.h"
#include "osapLoop.h"

vertex_t _root;
vertex_t* osap = &_root;

void osapSetup(void){
  _root.type = VT_TYPE_ROOT;
  _root.indice = 0;
  stackReset(&_root);
}

void osapLoop(void){
  osapRecursor(&_root);
}

// add general vertex i.e. vport
// assumes osap as root,
boolean osapAddVertex(vertex_t* vt) {
  if (_root.numChildren >= VT_MAXCHILDREN) {
    return false;
  } else {
    stackReset(vt);
    vt->indice = _root.numChildren;
    vt->parent = &_root;
    _root.children[_root.numChildren++] = vt;
    return true;
  }
}

vertex_t* osapBuildEndpoint(String name, boolean (*onData)(uint8_t* data, uint16_t ptr, uint16_t len), boolean (*beforeQuery)(void)){
  vertex_t* vt = new vertex_t; // allocates new to heap someplace, 
  vt->type = VT_TYPE_ENDPOINT;
  stackReset(vt);
  // add this to the system, 
  if(osapAddVertex(vt)){
    endpoint_t* ep = new endpoint_t;
    ep->onData = onData;
    ep->beforeQuery = beforeQuery;
    vt->loop = nullptr;
    vt->ep = ep;
    return vt;
  } else {
    delete vt;
    return nullptr;
  }
}