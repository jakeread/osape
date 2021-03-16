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

void osapSwitch(vertex_t* vt, uint8_t od, stackItem* item, uint16_t ptr, unsigned long now){
  // switch at pck[ptr + 1]
  ptr ++;
  uint8_t* pck = item->data;
  uint16_t len = item->len;
  switch(pck[ptr]){
    case PK_DEST: // instruction indicates pck is at vertex of destination, we try handler to rx data
      if(vt->onData == nullptr || vt->onData(pck, len)){
        #ifdef LOOP_DEBUG 
        sysError("destination copy");
        #endif 
        sysError("de " + String(item->arrivalTime));
        // no onData handler here, or it passed, so data copies in 
        memcpy(vt->data, pck, len);
        stackClearSlot(vt, od, item);
        break; // continue loop 
      } else {
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
          //sysError("ntcs");
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