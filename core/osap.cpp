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
#include "loop.h"

vertex_t _root;
vertex_t* osap = &_root;

void osapSetup(String name){
  _root.type = VT_TYPE_ROOT;
  _root.name = name;
  _root.indice = 0;
  stackReset(&_root);
}

void osapMainLoop(void){
  recursor(&_root);
}

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
