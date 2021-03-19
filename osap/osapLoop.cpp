/*
osap/osapLoop.cpp

main osap op: whips data vertex-to-vertex

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "osapLoop.h"
#include "../../drivers/indicators.h"
#include "../utils/syserror.h"

//#define LOOP_DEBUG

// recurse down vertex's children, 
// ... would be breadth-first, ideally 
void osapLoop(vertex_t* vt){
  osapHandler(vt);
  for(uint8_t child = 0; child < vt->numChildren; child ++){
    osapLoop(vt->children[child]);
  };
}

uint8_t _attemptedInstructions[16];
uint16_t _numAttemptedInstructions = 0;

void osapHandler(vertex_t* vt) {
  //sysError("handler " + vt->name);
  // run root's own loop code 
  if(vt->loop != nullptr) vt->loop();

  // time is now
  unsigned long now = millis();
  unsigned long at0;
  unsigned long at1;

  // handle origin stack, destination stack, in same manner 
  for(uint8_t od = 0; od < 2; od ++){
    // reset attempts from last cycle 
    _numAttemptedInstructions = 0;
    // try one handle per stack item, per loop:
    stackItem* items[vt->stackSize];
    uint8_t count = stackGetItems(vt, od, items, vt->stackSize);
    // want to track out-of-order issues to the loop. 
    if(count) at0 = items[0]->arrivalTime;
    for(uint8_t i = 0; i < count; i ++){
      // the item, and ptr
      stackItem* item = items[i];
      uint16_t ptr = 0;
      // check for decent ptr walk, 
      if(!ptrLoop(item->data, &ptr)){
        sysError("main loop bad ptr walk " + String(item->indice) + " " + String(ptr) + " len " + String(item->len));
        stackClearSlot(vt, od, item); // clears the msg 
        continue; 
      }
      // check timeouts, 
      #warning this should be above the ptrloop above for perf, is here for debug 
      if(item->arrivalTime + TIMES_STALE_MSG < now){
        sysError("T/O " + vt->name + " " + String(item->indice) + " " + String(item->data[ptr + 1]) + " " + String(item->arrivalTime));
        stackClearSlot(vt, od, item);
        continue;
      }
      // check FIFO order, 
      at1 = item->arrivalTime;
      if(at1 - at0 < 0) sysError("out of order " + String(at1) + " " + String(at0));
      at1 = at0;
      // handle it, 
      osapSwitch(vt, od, item, ptr, now);
    }
  } // end lp over origin / destination stacks 
}

void osapSwitch(vertex_t* vt, uint8_t od, stackItem* item, uint16_t ptr, unsigned long now){
  // switch at pck[ptr + 1]
  ptr ++;
  uint8_t* pck = item->data;
  uint16_t len = item->len;
  // don't try / fail same instructions twice, 
  for(uint8_t ins = 0; ins < _numAttemptedInstructions; ins ++){
    if(pck[ptr] == _attemptedInstructions[ins]) return;
  }
  // do things, 
  switch(pck[ptr]){
    case PK_DEST: // instruction indicates pck is at vertex of destination, we try handler to rx data
      if(vt->onData == nullptr || vt->onData(pck, ptr, len)){
        #ifdef LOOP_DEBUG 
        sysError("dexit " + String(item->arrivalTime));
        #endif         
        // no onData handler here, or it passed, so data copies in 
        memcpy(vt->data, pck, len);
        stackClearSlot(vt, od, item);
        break; // continue loop 
      } else {
        _attemptedInstructions[_numAttemptedInstructions] = PK_DEST;
        _numAttemptedInstructions ++;
        // onData returning false means endpoint is occupied / not ready for data, so we wait 
      }
      break;
    case PK_SIB_KEY: {  // instruction to pass to this sibling, 
      // need the indice, 
      uint16_t si;
      ptr ++;      
      ts_readUint16(&si, pck, &ptr);
      // can't do this if no parent, 
      if(vt->parent == nullptr){
        sysError("no parent for sib traverse");
        stackClearSlot(vt, od, item);
        break;
      }
      // nor if no target, 
      if(si >= vt->parent->numChildren){
        sysError("sib traverse oob");
        stackClearSlot(vt, od, item);
        break;
      }
      // now we can copy it in, only if there's space ahead to move it into 
      // this would be for other vertex's desitination slot, always 
      uint8_t space;
      if(stackEmptySlot(vt->parent->children[si], VT_STACK_DESTINATION)){
        #ifdef LOOP_DEBUG
        sysError("sib copy");
        #endif 
        ptr -= 4; // write in reversed instruction (reverse ptr to PK_PTR here)
        pck[ptr ++] = PK_SIB_KEY; // overwrite with instruction that would return to us, 
        ts_writeUint16(vt->indice, pck, &ptr);
        pck[ptr] = PK_PTR;
        // copy-in, set fullness, update time 
        stackLoadSlot(vt->parent->children[si], VT_STACK_DESTINATION, pck, len);
        // now og is clear 
        stackClearSlot(vt, od, item);
        // and we can finish here 
        break;
      } else {
        // actually, don't *have* to do this here, as we know osap shifts
        // items out of siblings' stacks **on this loop** and so 
        // is guaranteed not to happen mid-cycle, 
        /*
        _attemptedInstructions[_numAttemptedInstructions][0] = PK_SIB_KEY;
        _attemptedInstructions[_numAttemptedInstructions][1] = pck[ptr + 1];
        _attemptedInstructions[_numAttemptedInstructions][2] = pck[ptr + 2];
        _numAttemptedInstructions ++;
        */
        // destination stack at target is full, msg stays in place, next loop checks 
      }
    } 
    break; // end sib-fwd case, 
    case PK_PARENT_KEY:
    case PK_CHILD_KEY:
      // but is functionally identical to above, save small details... later 
      sysError("nav to parent / child unwritten");
      stackClearSlot(vt, od, item);
      break;
    case PK_PFWD_KEY:
      if(vt->type != VT_TYPE_VPORT || vt->cts == nullptr || vt->send == nullptr){
        sysError("pfwd to non-vport vertex");
        stackClearSlot(vt, od, item);
      } else {
        if(vt->cts(0)){ // walk ptr fwds, transmit, and clear the msg 
          pck[ptr - 1] = PK_PFWD_KEY;
          pck[ptr] = PK_PTR;
          vt->send(pck, len, 0);
          stackClearSlot(vt, od, item);
        } else {
          _attemptedInstructions[_numAttemptedInstructions] = PK_PFWD_KEY;
          _numAttemptedInstructions ++;
        }
      }
      break;
    case PK_BFWD_KEY:
      if(vt->type != VT_TYPE_VBUS || vt->cts == nullptr || vt->send == nullptr){
        sysError("bfwd to non-vbus vertex");
        stackClearSlot(vt, od, item);
      } else {
        // need tx rxaddr, for which drop on bus to tx to, 
        uint16_t rxAddr;
        ptr ++;      
        ts_readUint16(&rxAddr, pck, &ptr);
        if(vt->cts(rxAddr)){  // walk ptr fwds, transmit, and clear the msg 
          #ifdef LOOP_DEBUG 
          sysError("busf " + String(rxAddr));
          #endif
          sysError("be " + String(item->arrivalTime));
          ptr -= 4;
          pck[ptr ++] = PK_BFWD_KEY;
          ts_writeUint16(vt->ownRxAddr, pck, &ptr);
          pck[ptr] = PK_PTR;
          //logPacket(pck, len);
          vt->send(pck, len, rxAddr);
          stackClearSlot(vt, od, item);
        } else {
          _attemptedInstructions[_numAttemptedInstructions] = PK_BFWD_KEY;
          _numAttemptedInstructions ++;
        }
      }
      break;
    case PK_LLESCAPE_KEY:
    default:
      sysError("unrecognized ptr here");
      stackClearSlot(vt, od, item);
      break;
  } // end main switch 
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

boolean reverseRoute(uint8_t* pck, uint16_t rptr, uint8_t* repl, uint16_t* replyPtr){
  // so we should have here that 
  if(pck[rptr] != PK_PTR){
    sysError("rr: pck[ptr] != pk_ptr");
    return false;
  }
  // so we have a readptr (ptr) and writeptr (wptr)
  // we write *from the tail back* and read *from the tip in*
  uint16_t end = rptr;
  uint16_t wptr = rptr + 1;
  rptr = 0;
  // sequentially, at most 16 ops 
  for(uint8_t i = 0; i < 16; i ++){
    // end case: 
    if(rptr >= end){
      if(rptr != end){
        sysError("rr: rptr overruns end");
        return false;
      }
      repl[0] = PK_PTR;
      *replyPtr = rptr + 1;
      return true;
    }
    // switch each, 
    switch(pck[rptr]){
      case PK_PTR: // var is here 
        sysError("rr: find pck_ptr during walk");
        return false;
      case PK_SIB_KEY:
        wptr -= PK_SIB_INC;
        for(uint8_t j = 0; j < PK_SIB_INC; j ++){
          repl[wptr + j] = pck[rptr ++];
        }
        break;
      case PK_PARENT_KEY:
        wptr -= PK_PARENT_INC;
        for(uint8_t j = 0; j < PK_PARENT_INC; j ++){
          repl[wptr + j] = pck[rptr ++];
        }
        break;
      case PK_CHILD_KEY:
        wptr -= PK_CHILD_INC;
        for(uint8_t j = 0; j < PK_CHILD_INC; j ++){
          repl[wptr + j] = pck[rptr ++];
        }
        break;
      case PK_PFWD_KEY:
        wptr -= PK_PFWD_INC;
        for(uint8_t j = 0; j < PK_PFWD_INC; j ++){
          repl[wptr + j] = pck[rptr ++];
        }
        break;
      case PK_BFWD_KEY:
        wptr -= PK_BFWD_INC;
        for(uint8_t j = 0; j < PK_BFWD_INC; j ++){
          repl[wptr + j] = pck[rptr ++];
        }
        break;
      default:
        sysError("rr: default switch");
        return false;
    }
  }
}