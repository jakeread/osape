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
#include "../utils/syserror.h"

// to clear an item in the stack 
void stackClearSlot(vertex_t* vt, uint8_t od, uint8_t slot){
  vt->stackLengths[od][slot] = 0;
  switch(od){
    case VT_STACK_ORIGIN:
      if(vt->onOriginStackClear != nullptr) vt->onOriginStackClear(slot);
      break;
    case VT_STACK_DESTINATION:
      if(vt->onDestinationStackClear != nullptr) vt->onDestinationStackClear(slot);
      break;
    default:  // pretty unlikely 
      sysError("stack clear slot, od > 1, que?");
      break;
  }
}

// true if empty space in vertex stack
// *space = first empty 
boolean stackEmptySlot(vertex_t* vt, uint8_t od, uint8_t* slot){
  if(od > 1) return false;
  for(uint8_t s = 0; s < vt->stackSize; s ++){
    if(vt->stackLengths[od][s] == 0){
      *slot = s;
      return true;
    }
  }
  return false;
}

boolean stackNextMsg(vertex_t* vt, uint8_t od, uint8_t* slot){
  uint8_t s = vt->lastStackHandled[od];
  for (uint8_t i = 0; i < vt->stackSize; i++) {
    s++;
    if (s >= vt->stackSize) s = 0;
    if(vt->stackLengths[od][s] > 0){
      vt->lastStackHandled[od] = s;
      *slot = s;
      return true;
    }
  }
  return false;
}