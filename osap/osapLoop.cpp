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
#include "osapUtils.h"
#include "../../../indicators.h"
#include "../../../syserror.h"

//#define LOOP_DEBUG

// recurse down vertex's children, 
// ... would be breadth-first, ideally 
void osapRecursor(vertex_t* vt){
  osapHandler(vt);
  for(uint8_t child = 0; child < vt->numChildren; child ++){
    osapRecursor(vt->children[child]);
  };
}

void osapHandler(vertex_t* vt) {
  //sysError("handler " + vt->name);
  // run vertex's own loop code (with reference to self)
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
    // if(count) at0 = items[0]->arrivalTime; // pretty sure we can delete this as of 2021-12-31
    for(uint8_t i = 0; i < count; i ++){
      // the item, and ptr
      stackItem* item = items[i];
      uint16_t ptr = 0;
      // check timeouts, 
      if(item->arrivalTime + TIMES_STALE_MSG < now){
        sysError("T/O indice " + String(vt->indice) + " " + String(item->data[ptr + 1]) + " " + String(item->arrivalTime));
        stackClearSlot(vt, od, item);
        continue;
      }
      // check for decent ptr walk, 
      if(!ptrLoop(item->data, &ptr)){
        sysError("main loop bad ptr walk, from vt->indice: " + String(vt->indice) + " o/d: " + String(od) + " len: " + String(item->len));
        logPacket(item->data, item->len);
        stackClearSlot(vt, od, item); // clears the msg 
        continue; 
      }
      // check FIFO order, 
      // at1 = item->arrivalTime; // pretty sure we can delete this as of 2021-12-31
      // if(at1 - at0 < 0) sysError("out of order " + String(at1) + " " + String(at0));
      // at1 = at0;
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
  // do things, 
  switch(pck[ptr]){
    case PK_DEST: // instruction indicates pck is at vertex of destination, we try handler to rx data
      switch(vt->type){
        case VT_TYPE_ROOT:
        case VT_TYPE_MODULE:
        case VT_TYPE_VPORT:
        case VT_TYPE_VBUS:
          // all four: we are ignoring dms, wipe it:
          stackClearSlot(vt, od, item);
          break;
        case VT_TYPE_ENDPOINT: 
          {
            EP_ONDATA_RESPONSES resp = endpointHandler(vt, od, item, ptr);
            switch(resp){
              case EP_ONDATA_REJECT:
              case EP_ONDATA_ACCEPT: // in either case, msg is handled / out of stack, we can clear it 
                stackClearSlot(vt, od, item);
                break;
              case EP_ONDATA_WAIT: // not handled: want to wait in the stack, so update time & come back next 
                item->arrivalTime = now;
                break;
              default:
                sysError("on endpoint dest. handle, unknown ondata response");
                stackClearSlot(vt, od, item);
            } // end response switch 
          }
          break;
        default:
          // vertex has a 'type' we don't recognize, 
          // best I can do is deletion 
          sysError("unknown vertext type, when handling msg-at-destination");
          stackClearSlot(vt, od, item);
          break;
      } // end vt typeswitch 
      break; // end PK_DEST case 
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
      if(vt->parent == nullptr){
        ERROR(1, "requests traverse to parent from top level");
        stackClearSlot(vt, od, item);
      } else if(stackEmptySlot(vt->parent, VT_STACK_DESTINATION)){
        #ifdef LOOP_DEBUG
        sysError("copy to parent");
        #endif 
        ptr -= 1; // write in reversed instruction 
        pck[ptr ++] = PK_CHILD_KEY;
        ts_writeUint16(vt->indice, pck, &ptr);
        pck[ptr ++] = PK_PTR;
        // copy-in, set fullness and update time 
        stackLoadSlot(vt->parent, VT_STACK_DESTINATION, pck, len);
        stackClearSlot(vt, od, item);
      }
      break;
    case PK_CHILD_KEY:
      // find child 
      uint16_t ci;
      ptr ++;
      ts_readUint16(&ci, pck, &ptr);
      // can't do it w/o the child, 
      if(vt->numChildren <= ci){
        ERROR(1, "no child at this indice " + String(ci));
        stackClearSlot(vt, od, item);
      } else if (stackEmptySlot(vt->children[ci], VT_STACK_DESTINATION)){
        #ifdef LOOP_DEBUG
        sysError("copy to child");
        #endif 
        ptr -= 4;
        pck[ptr ++] = PK_PARENT_KEY;
        ts_writeUint16(0, pck, &ptr); // parent 'index' used bc packet length should be symmetric 
        pck[ptr ++] = PK_PTR;
        // do the copy-in, set fullness, etc 
        stackLoadSlot(vt->children[ci], VT_STACK_DESTINATION, pck, len);
        stackClearSlot(vt, od, item);
      }
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
          // failed to pfwd this turn, code will return here next go-round 
        }
      }
      break;
    case PK_BFWD_KEY:
      if(vt->type != VT_TYPE_VBUS || vt->cts == nullptr || vt->send == nullptr){
        sysError("bfwd to non-vbus vertex");
        logPacket(item->data, item->len);
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
          //sysError("be " + String(item->arrivalTime));
          ptr -= 4;
          pck[ptr ++] = PK_BFWD_KEY;
          ts_writeUint16(vt->ownRxAddr, pck, &ptr);
          pck[ptr] = PK_PTR;
          //logPacket(pck, len);
          vt->send(pck, len, rxAddr);
          stackClearSlot(vt, od, item);
        } else {
          // failed to bfwd this turn, code will return here next go-round 
        }
      }
      break;
    case PK_SCOPE_REQ_KEY: 
      {
        // so we want to write a brief packet in here, and we should do it str8 into the same item, 
        // we need to reverse the route into a temp object:
        uint8_t route[VT_SLOTSIZE];
        uint16_t wptr = 0;
        if(!reverseRoute(item->data, ptr - 1, route, &wptr)){
          ERROR(1, "reverse route badness at scope request");
          stackClearSlot(vt, od, item);
        }
        // because reverse route assumes dest, segsize / checksum, we actually want...
        wptr -= 3;
        // now we can write in:
        // recall that item->data is the stack-item's little msg block... 
        memcpy(item->data, route, wptr);
        // and write response key
        item->data[wptr ++] = PK_SCOPE_RES_KEY;
        // id from the REQ, should actually be untouched, right?
        wptr ++;
        // want 2 of these 
        uint16_t rptr = wptr;
        // collect new timeTag: vertex keeps 'last-time-scoped' state
        // here we write-in to the response our *previous* time-scoped, and replace that w/ this requests' tag 
        uint32_t newScopeTimeTag;
        ts_readUint32(&newScopeTimeTag, item->data, &rptr);
        ts_writeUint32(vt->scopeTimeTag, item->data, &wptr);
        vt->scopeTimeTag = newScopeTimeTag;
        // our type 
        item->data[wptr ++] = vt->type;
        // our own indice, our # of siblings, our # of children:
        ts_writeUint16(vt->indice, item->data, &wptr);
        if(vt->parent != nullptr){
          ts_writeUint16(vt->parent->numChildren, item->data, &wptr);
        } else {
          ts_writeUint16(0, item->data, &wptr);
        }
        ts_writeUint16(vt->numChildren, item->data, &wptr);
        // and our name... 
        ts_writeString(vt->name, item->data, &wptr);
        /*
        switch(vt->type){
          case VT_TYPE_ENDPOINT:
            ts_writeString("embedded-endpoint", item->data, &wptr);
            break;
          case VT_TYPE_ROOT:
            ts_writeString("embedded-root", item->data, &wptr);
            break;
          case VT_TYPE_VPORT:
            ts_writeString("embedded-vport", item->data, &wptr);
            break;
          default:
            ts_writeString("embedded-vertex", item->data, &wptr);
            break;
        }
        */
        // ok then, we can reset this item, basically:
        item->len = wptr;
        item->arrivalTime = millis();
        // osap will pick it up next loop, ship it back. 
      }
      break;
    case PK_SCOPE_RES_KEY:
    case PK_LLESCAPE_KEY:
    default:
      sysError("unrecognized ptr here at " + String(ptr) + ": " + String(pck[ptr]));
      logPacket(pck, len);
      stackClearSlot(vt, od, item);
      break;
  } // end main switch 
}