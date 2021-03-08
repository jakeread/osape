/*
osap/osap_loop.cpp

main osap op: whips data vertex-to-vertex

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "osap_loop.h"
#include "../../drivers/indicators.h"
#include "../utils/syserror.h"

// recurse down vertex's children, 
// ... would be breadth-first, ideally 
void osapLoop(vertex_t* vt){
  osapHandler(vt);
  for(uint8_t child = 0; child < vt->numChildren; child ++){
    osapLoop(vt->children[child]);
  };
}

void osapHandler(vertex_t* vt) {
  //sysError("handler " + vt->name);
  // run root's own loop code 
  if(vt->loop != nullptr) vt->loop();

  // find next unhandled message in the stack 
  uint8_t s = vt->lastHandled;
  boolean capture = false;
  for (uint8_t i = 0; i < vt->stackSize; i++) {
    s++;
    if (s >= vt->stackSize) s = 0;
    if(vt->stackLen[s] > 0){
      capture = true;
      vt->lastHandled = s;
      break; // escape and continue with this 
    }
  }

  // if no item in stack here, continue search 
  if(!capture){
    //sysError("no capture");
    return;
  }

  sysError(vt->name + " cap " + String(s));

  // have ahn msg, is at vt->stack[s]
  // first check if it's timed out 
  unsigned long now = millis();
  if(vt->stackArrivalTimes[s] + TIMES_STALE_MSG < now){
    sysError("timeout pck");
    vt->stackLen[s] = 0;
    return;
  }

  // now carry on, try to handle it, 
  uint8_t* pck = vt->stack[s];
  uint16_t ptr = 0;
  uint16_t len = vt->stackLen[s];
  // could do a potential speedup where the vertex tracks last known ptr posotion 
  if(!ptrLoop(pck, &ptr)){
    sysError("main loop bad ptr walk " + String(ptr) + " len " + String(len));
    vt->stackLen[s] = 0; // clears the msg 
    return; 
  }

  // switch at pck[ptr + 1]
  ptr ++;
  switch(pck[ptr]){
    case PK_DEST:
      if(vt->onData == nullptr || vt->onData(pck, len)){
        sysError("destination copy");
        // no onData handler here, or it passed, so data copies in 
        memcpy(vt->data, pck, len);
        vt->stackLen[s] = 0;
        return;
      } else {
        // onData returning false means endpoint is occupied / not ready for data, so we wait  
      }
      break;
    case PK_SIB_KEY: {
      // need the indice, 
      uint16_t si;
      ptr ++;      
      ts_readUint16(&si, pck, &ptr);
      // can't do this if no parent, 
      if(vt->parent == nullptr){
        sysError("no parent for sib traverse");
        vt->stackLen[s] = 0;
        return;
      }
      // nor if no target, 
      if(si >= vt->parent->numChildren){
        sysError("sib traverse oob");
        vt->stackLen[s] = 0;
        return;
      }
      // now we can copy it in, only if there's space ahead to move it into 
      uint8_t space;
      if(vertexSpace(vt->parent->children[si], &space)){
        sysError("sib copy");
        ptr -= 4; // write in reversed instruction (reverse ptr to PK_PTR here)
        pck[ptr ++] = PK_SIB_KEY; // overwrite with instruction that would return to us, 
        ts_writeUint16(vt->indice, pck, &ptr);
        pck[ptr] = PK_PTR;
        // copy-in, set fullness, update time 
        memcpy(vt->parent->children[si]->stack[space], pck, len);
        vt->parent->children[si]->stackLen[space] = len;
        vt->parent->children[si]->stackArrivalTimes[space] = now;
        // now og is clear 
        vt->stackLen[s] = 0;
        // and we can finish here 
        return;
      } else {
        // the wait term, 
      }
    } 
    break; // end sib-fwd case, 
    case PK_PARENT_KEY:
    case PK_CHILD_KEY:
      // but is functionally identical to above, save small details... later 
      sysError("nav to parent / child unwritten");
      vt->stackLen[s] = 0;
      return;
    case PK_PFWD_KEY:
      if(vt->type != VT_TYPE_VPORT || vt->cts == nullptr || vt->send == nullptr){
        sysError("pfwd to non-vport vertex");
        vt->stackLen[s] = 0;
        return;
      }
      if(vt->cts(0)){
        pck[ptr - 1] = PK_PFWD_KEY;
        pck[ptr] = PK_PTR;
        vt->send(pck, len, 0);
        return;
      }
      break;
    case PK_LLESCAPE_KEY:
    default:
      sysError("unrecognized ptr here");
      vt->stackLen[s] = 0;
      return;
  }
}

boolean ptrLoop(uint8_t* pck, uint16_t* pt){
  uint16_t ptr = *pt;
  for(uint8_t i = 0; i < 16; i ++){
    //sysError(String(ptr));
    switch(pck[ptr]){
      case PK_PTR: // var is here 
        *pt = ptr;
        return true;
      case PK_SIB_KEY:
        ptr += PK_SIB_INC;
        break;
      case PK_PARENT_KEY:
        ptr += PK_PARENT_INC;
        break;
      case PK_CHILD_KEY:
        ptr += PK_CHILD_INC;
        break;
      case PK_PFWD_KEY:
        ptr += PK_PFWD_INC;
        break;
      case PK_BFWD_KEY:
        ptr += PK_BFWD_INC;
        break;
      default:
        return false;
    }
  }
  // case where no ptr after 16 hops, 
  return false;
}