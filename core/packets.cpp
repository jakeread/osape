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

boolean findPtr(uint8_t* pck, uint16_t* pt){
  // 1st instruction is always at pck[4], pck[0][1] == ttl, pck[2][3] == segSize 
  uint16_t ptr = 4;
  // there's a potential speedup where we assume given *pt is already incremented somewhat, 
  // maybe shaves some ns... but here we just look fresh every time, 
  for(uint8_t i = 0; i < 16; i ++){
    switch(PK_READKEY(pck[ptr])){
      case PK_PTR: // var is here 
        *pt = ptr;
        return true;
      case PK_SIB:
      case PK_PARENT:
      case PK_CHILD:
      case PK_PFWD:
      case PK_BFWD:
      case PK_BBRD:
        ptr += 2;
        break;
      default:
        return false;
    }
  }
  // case where no ptr after 16 hops, 
  return false;
}

uint16_t writeDatagram(uint8_t* gram, uint16_t maxGramLength, Route* route, uint8_t* payload, uint16_t payloadLen){
  uint16_t wptr = 0;
  ts_writeUint16(route->ttl, gram, &wptr);
  ts_writeUint16(route->segSize, gram, &wptr);
  memcpy(&(gram[wptr]), route->path, route->pathLen);
  wptr += route->pathLen;
  if(wptr + payloadLen > route->segSize){
    #ifdef OSAP_DEBUG
    ERROR(1, "writeDatagram asked to write packet that exceeds segSize, bailing");
    #endif 
    return 0;
  }
  memcpy(&(gram[wptr]), payload, payloadLen);
  wptr += payloadLen;
  return wptr;
}

// original gram, payload, len, 
uint16_t writeReply(uint8_t* ogGram, uint8_t* gram, uint16_t maxGramLength, uint8_t* payload, uint16_t payloadLen){
  // 1st up, we can straight copy the 1st 4 bytes, 
  memcpy(ogGram, gram, 4);
  // now find a ptr, 
  uint16_t ptr = 0;
  if(!findPtr(ogGram, &ptr)){
    #ifdef OSAP_DEBUG
    ERROR(1, "writeReply can't find the pointer...");
    #endif 
    return 0;
  }
  // do we have enough space? it's the minimum of the allowed segsize & stated maxGramLength, 
  maxGramLength = min(maxGramLength, ts_readUint16(ogGram, 2));
  if(ptr + 1 + payloadLen > maxGramLength){
    #ifdef OSAP_DEBUG
    ERROR(1, "writeReply asked to write packet that exceeds maxGramLength, bailing");
    #endif 
    return 0;
  }
  // write the payload in, apres-pointer, 
  memcpy(&(ogGram[ptr + 1]), payload, payloadLen);
  // now we can do a little reversing... 
  uint16_t wptr = 4;
  uint16_t end = ptr;
  uint16_t rptr = ptr;
  // 1st byte... the ptr, 
  gram[wptr ++] = PK_PTR;
  // now for a max 16 steps, 
  for(uint8_t h = 0; h < 16; h ++){
    if(wptr >= end) break;
    rptr -= 2;
    switch(PK_READKEY(ogGram[rptr])){
      case PK_SIB:
      case PK_PARENT:
      case PK_CHILD:
      case PK_PFWD:
      case PK_BFWD:
      case PK_BBRD:
        gram[wptr ++] = ogGram[rptr];
        gram[wptr ++] = ogGram[rptr + 1];
        break;
      default:
        #ifdef OSAP_DEBUG
        ERROR(1, "writeReply cannot reverse this packet, bailing");
        #endif 
        return 0;
    }
  } // end thru-loop, 
  // it's written, return the len  // we had gram[ptr] = PK_PTR, so len was ptr + 1, then added payloadLen, 
  return end + 1 + payloadLen;
}

/*
PK.writeReply = (ogPck, payload) => {
  // find the pointer, 
  let ptr = PK.findPtr(ogPck)
  if (!ptr) throw new Error(`during reply-write, couldn't find the pointer...`);
  // our new datagram will be this long (ptr is location of ptr, len is there + 1) + the payload length, so 
  let datagram = new Uint8Array(ptr + 1 + payload.length)
  // we're using the OG ttl and segsize, so we can just write that in, 
  datagram.set(ogPck.subarray(0, 4))
  // and also write in the payload, which will come after the ptr's current position, 
  datagram.set(payload, ptr + 1)
  // now we want to do the walk-to-ptr, reversing... 
  // we write at the head of the packet, whose first byte is the pointer, 
  let wptr = 4
  datagram[wptr++] = PK.PTR
  // don't write past here, 
  let end = ptr 
  // read from the back, 
  let rptr = ptr
  walker: for (let h = 0; h < 16; h++) {
    if(wptr >= end) break walker;
    rptr -= 2
    switch (TS.readKey(ogPck, rptr)) {
      case PK.SIB:
      case PK.PARENT:
      case PK.CHILD:
      case PK.PFWD:
      case PK.BFWD:
      case PK.BBRD:
        // actually we can do the same action for each of these keys, 
        datagram.set(ogPck.subarray(rptr, rptr + 2), wptr)
        wptr += 2
        break;
      default:
        throw new Error(`during writeReply route reversal, encountered unpredictable key ${ogPck[rptr]}`)
    }
  }
  // that's it, 
  return datagram
}
*/