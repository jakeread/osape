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

// clears item in the tail, increments tail 
void stackClearSlot(vertex_t* vt, uint8_t od){
  if(od > 1) return; 
  vt->stackTail[od] ++;
  if(vt->stackTail[od] >= vt->stackSize){
    vt->stackTail[od] = 0;
  }
  //vt->stackLengths[od][slot] = 0;
  switch(od){
    case VT_STACK_ORIGIN:
      #warning these set to zero for now, 
      if(vt->onOriginStackClear != nullptr) vt->onOriginStackClear(0);
      break;
    case VT_STACK_DESTINATION:
      if(vt->onDestinationStackClear != nullptr) vt->onDestinationStackClear(0);
      break;
    default:  // pretty unlikely 
      sysError("stack clear slot, od > 1, que?");
      break;
  }
}

// true if there's any space in the stack, 
boolean stackEmptySlot(vertex_t* vt, uint8_t od){
  if(od > 1) return false;
  // if head + 1 == tail, is full 
  uint8_t head = vt->stackHead[od] + 1;
  if(head >= vt->stackSize) head = 0;
  if(head == vt->stackTail[od]){
    return false;
  } else {
    return true;
  }
  /*
  uint8_t s = vt->lastStackHandled[od];
  for(uint8_t i = 0; i < vt->stackSize; i ++){
    s ++;
    if(s >= vt->stackSize) s = 0;
    if(vt->stackLengths[od][s] == 0){
      *slot = s;
      return true;
    }
  }
  return false;
  */
}

// loads data into stack 
void stackLoadSlot(vertex_t* vt, uint8_t od, uint8_t* data, uint16_t len){
  if(od > 1) return; // bad od, lost data 
  // head is always @ next empty space,
  uint8_t head = vt->stackHead[od];
  memcpy(vt->stack[od][head], data, len);
  vt->stackLengths[od][head] = len;
  vt->stackArrivalTimes[od][head] = millis();
  // now increment to next empty, head will be set to overwrite old if was full 
  head ++;
  if(head >= vt->stackSize){
    head = 0;
  }
  vt->stackHead[od] = head;
}

// true if there is msg to pull, and reports space: 
boolean stackNextMsg(vertex_t* vt, uint8_t od, uint8_t* slot){
  // guard bad od 
  if(od > 1) return false;
  // when tail == head, no msgs 
  if(vt->stackTail[od] == vt->stackHead[od]) return false; 
  // slot to pull is tail, tail only 
  *slot = vt->stackTail[od];
  return true;
  /*
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
  */
}