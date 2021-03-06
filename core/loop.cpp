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

#include "loop.h"
#include "packets.h"
#ifdef OSAP_DEBUG 
#include "./osap_debug.h"
#endif 

//#define LOOP_DEBUG

// ... would be breadth-first, ideally 
void loopRecursor(Vertex* vt){
  // reset loop state, 
  vt->incomingItemCount = 0;
  // vertex loop code, but it's circular if parent calls itself 
  if(vt->parent != nullptr) vt->loop();
  // now recurse, 
  for(uint8_t child = 0; child < vt->numChildren; child ++){
    loopRecursor(vt->children[child]);
  };
}

// recurse again, this time calling handler, 
void setupRecursor(Vertex* vt){
  setupHandler(vt);
  for(uint8_t child = 0; child < vt->numChildren; child ++){
    setupRecursor(vt->children[child]);
  };
}

void setupHandler(Vertex* vt) {
  // time is now
  unsigned long now = millis();
  // handle origin stack, destination stack, in same manner 
  for(uint8_t od = 0; od < 2; od ++){
    // try one handle per stack item, per loop:
    stackItem* items[vt->stackSize];
    uint8_t count = stackGetItems(vt, od, items, vt->stackSize);
    for(uint8_t i = 0; i < count; i ++){
      // the item, and ptr
      stackItem* item = items[i];
      uint16_t ptr = 0;
      // check timeouts, 
      if(item->arrivalTime + TIMES_STALE_MSG < now){
        #ifdef OSAP_DEBUG
        ERROR(3, "setup loop T/O indice " + String(vt->indice) + " " + vt->name + " " + String(item->arrivalTime));
        #endif 
        stackClearSlot(vt, od, item);
        continue;
      }
      // check for decent ptr walk, 
      if(!ptrLoop(item->data, &ptr)){
        #ifdef OSAP_DEBUG
        ERROR(2, "main loop bad ptr walk, from vt->indice: " + String(vt->indice) + vt->name + " o/d: " + String(od) + " len: " + String(item->len));
        logPacket(item->data, item->len);
        #endif 
        stackClearSlot(vt, od, item); // clears the msg 
        continue; 
      }
      // count transmitters, 
      setupSwitch(vt, od, item, ptr, now);
    }
  } // end lp over origin / destination stacks 
}

// this'll be the bigger switch: punts bad messages, etc. 
void setupSwitch(Vertex* vt, uint8_t od, stackItem* item, uint16_t ptr, unsigned long now){
  // switch at pck[ptr + 1]
  ptr ++;
  uint8_t* pck = item->data;
  uint16_t len = item->len;
  item->ptr = ptr; // for the transfer switch, 
  // do things, 
  switch(pck[ptr]){
    case PK_DEST: // instruction indicates pck is at vertex of destination, we try handler to rx data
      #warning could speedup w/ stackItem cache as "atDest" 
      break; // end PK_DEST case 
    case PK_SIB_KEY: {  // instruction to pass to this sibling, 
      // need the indice, 
      uint16_t si;
      ptr ++;      
      ts_readUint16(&si, pck, &ptr);
      // can't do this if no parent, 
      if(vt->parent == nullptr){
        #ifdef OSAP_DEBUG
        ERROR(1, "no parent for sib traverse");
        #endif 
        stackClearSlot(vt, od, item);
        break;
      }
      // nor if no target, 
      if(si >= vt->parent->numChildren){
        #ifdef OSAP_DEBUG
        ERROR(1, "sib traverse oob");
        #endif 
        stackClearSlot(vt, od, item);
        break;
      }
      // passes tests, track us at target source, 
      vt->parent->children[si]->incomingItems[vt->parent->children[si]->incomingItemCount ++] = item;
      break;
    } // end sib-fwd case 
    case PK_PARENT_KEY:
      if(vt->parent == nullptr){
        #ifdef OSAP_DEBUG
        ERROR(1, "requests traverse to parent from top level");
        #endif 
        stackClearSlot(vt, od, item);
        break;
      } 
      vt->parent->incomingItems[vt->parent->incomingItemCount ++] = item;
      break; // end parent-fwd case 
    case PK_CHILD_KEY:
      // find child 
      uint16_t ci;
      ptr ++;
      ts_readUint16(&ci, pck, &ptr);
      // can't do it w/o the child, 
      if(vt->numChildren <= ci){
        #ifdef OSAP_DEBUG
        ERROR(1, "no child at this indice " + String(ci));
        #endif 
        stackClearSlot(vt, od, item);
        break;
      } 
      vt->children[ci]->incomingItems[vt->children[ci]->incomingItemCount ++] = item;
      break; // end child-fwd case 
      // ---------------------------------------- Network Switches 
    case PK_PFWD_KEY:
      if(vt->vport == nullptr){
        #ifdef OSAP_DEBUG
        ERROR(1, "pfwd to non-vport vertex");
        #endif 
        stackClearSlot(vt, od, item);
      } else {
        if(vt->vport->cts()){ // walk ptr fwds, transmit, and clear the msg 
          pck[ptr - 1] = PK_PFWD_KEY;
          pck[ptr] = PK_PTR;
          vt->vport->send(pck, len);
          stackClearSlot(vt, od, item);
        } else {
          // failed to pfwd this turn, code will return here next go-round 
        }
      }
      break;
    case PK_BFWD_KEY:
      if(vt->vbus == nullptr){
        #ifdef OSAP_DEBUG
        ERROR(1, "bfwd to non-vbus vertex");
        logPacket(item->data, item->len);
        #endif 
        stackClearSlot(vt, od, item);
      } else {
        // need tx rxaddr, for which drop on bus to tx to, 
        uint16_t rxAddr;
        ptr ++;      
        ts_readUint16(&rxAddr, pck, &ptr);
        if(vt->vbus->cts(rxAddr)){  // walk ptr fwds, transmit, and clear the msg 
          #ifdef OSAP_DEBUG
          #ifdef LOOP_DEBUG 
          DEBUG("busf " + String(rxAddr));
          #endif
          #endif 
          ptr -= 4;
          pck[ptr ++] = PK_BFWD_KEY;
          ts_writeUint16(vt->vbus->ownRxAddr, pck, &ptr);
          pck[ptr] = PK_PTR;
          //logPacket(pck, len);
          vt->vbus->send(pck, len, rxAddr);
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
          #ifdef OSAP_DEBUG
          ERROR(1, "reverse route badness at scope request");
          #endif 
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
        // ok then, we can reset this item, basically:
        item->len = wptr;
        item->arrivalTime = now;
        // osap will pick it up next loop, ship it back. 
      }
      break;
    case PK_SCOPE_RES_KEY:
    case PK_LLESCAPE_KEY:
    default:
      #ifdef OSAP_DEBUG
      ERROR(1, "unrecognized ptr here at " + String(ptr) + ": " + String(pck[ptr]));
      logPacket(pck, len);
      #endif 
      stackClearSlot(vt, od, item);
      break;
  } // end main switch 
}

void transferRecursor(Vertex* vt){
  transferHandler(vt);
  for(uint8_t child = 0; child < vt->numChildren; child ++){
    transferRecursor(vt->children[child]);
  };
}

void transferHandler(Vertex* vt){
  // this would be like... 
  if(vt->incomingItemCount == 0) return;
  // then do... item to serve,
  uint16_t its = vt->lastIncomingServed;
  // serve 'em, starting w/ next-from-last-served, 
  for(uint16_t i = 0; i < vt->incomingItemCount; i ++){
    its ++;
    if(its >= vt->incomingItemCount) its = 0;
    if(transferSwitch(vt, vt->incomingItems[its])){
      vt->lastIncomingServed = its;
    }
  }
}

// we're only concerned here w/ transfers vertex-to-vertex, 
// pls note: vt here is *sink* 
boolean transferSwitch(Vertex* vt, stackItem* item){
  // mmkheeey, 
  uint8_t* pck = item->data;
  uint16_t len = item->len;
  uint16_t ptr = item->ptr;
  // do things, 
  switch(pck[item->ptr]){
    case PK_SIB_KEY: {  // instruction to pass to this sibling, 
      // need the indice, 
      uint16_t si;
      ptr ++;
      ts_readUint16(&si, pck, &ptr);
      // now we can copy it in, only if there's space ahead to move it into 
      if(stackEmptySlot(item->vt->parent->children[si], VT_STACK_DESTINATION)){
        #ifdef OSAP_DEBUG
        #ifdef LOOP_DEBUG
        DEBUG("sib copy to " + String(si) + " from " + String(item->vt->indice));
        #endif 
        #endif 
        ptr -= 4; // write in reversed instruction (reverse ptr to PK_PTR here)
        pck[ptr ++] = PK_SIB_KEY; // overwrite with instruction that would return to us, 
        ts_writeUint16(item->vt->indice, pck, &ptr);
        pck[ptr] = PK_PTR;
        // copy-in, set fullness, update time 
        stackLoadSlot(item->vt->parent->children[si], VT_STACK_DESTINATION, pck, len);
        // now og is clear 
        stackClearSlot(item->vt, item->od, item);
        // and we can finish here 
        return true; 
      } else {
        return false; 
        // destination stack at target is full, msg stays in place, next loop checks 
      }
    }
    case PK_PARENT_KEY:
      #warning ... each of these, we could be "sure" that setup means we are looking to load into vt 
      if(stackEmptySlot(item->vt->parent, VT_STACK_DESTINATION)){
        #ifdef OSAP_DEBUG 
        #ifdef LOOP_DEBUG
        DEBUG("copy to parent");
        #endif 
        #endif 
        ptr -= 1; // write in reversed instruction 
        pck[ptr ++] = PK_CHILD_KEY;
        ts_writeUint16(item->vt->indice, pck, &ptr);
        pck[ptr ++] = PK_PTR;
        // copy-in, set fullness and update time 
        stackLoadSlot(item->vt->parent, VT_STACK_DESTINATION, pck, len);
        stackClearSlot(item->vt, item->od, item);
        return true;
      } else {
        return false;
      }
    case PK_CHILD_KEY:
      // find child 
      uint16_t ci;
      ptr ++;
      ts_readUint16(&ci, pck, &ptr);
      if (stackEmptySlot(item->vt->children[ci], VT_STACK_DESTINATION)){
        #ifdef OSAP_DEBUG
        #ifdef LOOP_DEBUG
        DEBUG("copy to child");
        #endif 
        #endif 
        ptr -= 4;
        pck[ptr ++] = PK_PARENT_KEY;
        ts_writeUint16(0, pck, &ptr); // parent 'index' used bc packet length should be symmetric 
        pck[ptr ++] = PK_PTR;
        // do the copy-in, set fullness, etc 
        stackLoadSlot(item->vt->children[ci], VT_STACK_DESTINATION, pck, len);
        stackClearSlot(item->vt, item->od, item);
        return true;
      } else {
        return false; 
      }
    default:
      #ifdef OSAP_DEBUG
      ERROR(1, "unrecognized ptr in transfer switch here at " + String(ptr) + ": " + String(pck[ptr]));
      logPacket(pck, len);
      #endif 
      stackClearSlot(item->vt, item->od, item);
      return true;
  } // end transfer switch 
}