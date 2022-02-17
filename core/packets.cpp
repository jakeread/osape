/*
osap/osapUtils.cpp

common routines 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "packets.h"
#include "ts.h"
#ifdef OSAP_DEBUG
#include "./osap_debug.h"
#endif 

boolean ptrLoop(uint8_t* pck, uint16_t* pt){
  uint16_t ptr = *pt;
  for(uint8_t i = 0; i < 16; i ++){
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

// pck: bytes, rptr: read ptr (should start at pck[rptr] = PTR / 88)
// repl: reply bytes, replyPtr ... ptr after setup 
boolean reverseRoute(uint8_t* pck, uint16_t rptr, uint8_t* repl, uint16_t* replyPtr){
  // so we should have here that 
  if(pck[rptr] != PK_PTR){
    #ifdef OSAP_DEBUG
    ERROR(1, "rr: pck[ptr] != pk_ptr");
    #endif 
    return false;
  }
  // the tail is identical: dest & segsize following 
  for(uint8_t i = 3; i > 0; i --){
      repl[rptr + 4 - i] = pck[rptr + 4 - i];
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
        #ifdef OSAP_DEBUG
        ERROR(1, "rr: rptr overruns end");
        #endif 
        return false;
      }
      // start is pointer, 
      repl[0] = PK_PTR;
      // end is past ptr, + dest key, + checksum (2), + 1 so 
      // first write repl[rptr] = nextByte 
      *replyPtr = rptr + 4;
      return true;
    }
    // switch each, 
    switch(pck[rptr]){
      case PK_PTR: // var is here 
        #ifdef OSAP_DEBUG
        ERROR(1, "rr: find pck_ptr during walk");
        #endif 
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
        #ifdef OSAP_DEBUG
        ERROR(1, "rr: default switch");
        #endif 
        return false;
    }
  }
  // if reach end of ptr walk but no exit, badness
  return false;
}