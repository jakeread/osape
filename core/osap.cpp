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
#ifdef OSAP_DEBUG
#include "./osap_debug.h"
#endif 

void osapMainLoop(Vertex* root){
  recursor(root);
}

// user musn't fk tree 
boolean osapAddVertex(Vertex* parent, Vertex* child) {
  if (parent->numChildren >= VT_MAXCHILDREN) {
    return false;
  } else {
    child->indice = parent->numChildren;
    child->parent = parent;
    parent->children[parent->numChildren ++] = child;
    return true;
  }
}
