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
#include "packets.h"
#include "../utils/cobs.h"

// stash most recents, and counts, 
uint32_t errorCount = 0;
uint32_t debugCount = 0;
// strings...
unsigned char latestError[VT_SLOTSIZE];
unsigned char latestDebug[VT_SLOTSIZE];
uint16_t latestErrorLen = 0;
uint16_t latestDebugLen = 0;

OSAP::OSAP(String _name) : Vertex(_name){};

void OSAP::loop(void){
  // this is the root, so we kick all of the internal net operation from here 
  osapLoop(this);
}

void OSAP::destHandler(stackItem* item, uint16_t ptr){
  // classic switch on 'em 
  // item->data[ptr] == PK_PTR, ptr + 1 == PK_DEST, ptr + 2 == ROOT_KEY, ptr + 3 = ID (if ack req.) 
  uint16_t wptr = 0;
  uint16_t len = 0;
  switch(item->data[ptr + 2]){
    case RT_ERR_QUERY:
      // stuff error count & latest message 
      payload[wptr ++] = PK_DEST;
      payload[wptr ++] = RT_ERR_RES;
      payload[wptr ++] = item->data[ptr + 3];
      ts_writeUint32(errorCount, payload, &wptr);
      // aye... there's more critical thinking to be done on packet-size errors, 
      // previously I've just written lots of "bailing..." error codes in place, i.e. in 
      // writeReply, but these *are* the error codes, haha, so we should just truncate them, 
      // and writeReply, etc, is perhaps where we need to do that: since here we don't have 
      // upfront info w/r/t i.e. how long the og route is, 
      ts_writeString(latestError, latestErrorLen, payload, &wptr, VT_SLOTSIZE / 2);
      // that's the payload, I figure, 
      len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, wptr);
      stackClearSlot(item);
      stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
      break;
    case RT_DBG_QUERY:
            // stuff error count & latest message 
      payload[wptr ++] = PK_DEST;
      payload[wptr ++] = RT_DBG_RES;
      payload[wptr ++] = item->data[ptr + 3];
      ts_writeUint32(debugCount, payload, &wptr);
      // aye... there's more critical thinking to be done on packet-size errors, 
      // previously I've just written lots of "bailing..." error codes in place, i.e. in 
      // writeReply, but these *are* the error codes, haha, so we should just truncate them, 
      // and writeReply, etc, is perhaps where we need to do that: since here we don't have 
      // upfront info w/r/t i.e. how long the og route is, 
      ts_writeString(latestDebug, latestDebugLen, payload, &wptr, VT_SLOTSIZE / 2);
      // that's the payload, I figure, 
      len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, wptr);
      stackClearSlot(item);
      stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
      break;
      break;
    default:
      OSAP::error("unrecognized key to root node " + String(item->data[ptr + 2]));
      stackClearSlot(item);
      break;
  }
}

uint8_t errBuf[255];
uint8_t errBufEncoded[255];

void debugPrint(String msg){
  // whatever you want,
  uint32_t len = msg.length();
  // max this long, per the serlink bounds 
  if(len + 9 > 255) len = 255 - 9;
  // header... 
  errBuf[0] = len + 8;  // len, key, cobs start + end, strlen (4) 
  errBuf[1] = 172;      // serialLink debug key 
  errBuf[2] = len & 255;
  errBuf[3] = (len >> 8) & 255;
  errBuf[4] = (len >> 16) & 255;
  errBuf[5] = (len >> 24) & 255;
  msg.getBytes(&(errBuf[6]), len + 1);
  // encode from 2, leaving the len, key header... 
  size_t ecl = cobsEncode(&(errBuf[2]), len + 4, errBufEncoded);
  // what in god blazes ? copy back from encoded -> previous... 
  memcpy(&(errBuf[2]), errBufEncoded, ecl);
  // set tail to zero, to delineate, 
  errBuf[errBuf[0] - 1] = 0;
  // direct escape 
  Serial.write(errBuf, errBuf[0]);
}

void OSAP::error(String msg, OSAPErrorLevels lvl){
  //const char* str = msg.c_str();
  msg.getBytes(latestError, VT_SLOTSIZE);
  latestErrorLen = msg.length();
  errorCount ++;
  debugPrint(msg);
}

void OSAP::debug(String msg, OSAPDebugStreams stream){
  msg.getBytes(latestDebug, VT_SLOTSIZE);
  latestDebugLen = msg.length();
  debugCount ++;
  debugPrint(msg);
}