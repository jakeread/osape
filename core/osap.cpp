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

// stash most recents, and counts, and high water mark, 
uint32_t OSAP::loopItemsHighWaterMark = 0;
uint32_t errorCount = 0;
uint32_t debugCount = 0;
// strings...
unsigned char latestError[VT_SLOTSIZE];
unsigned char latestDebug[VT_SLOTSIZE];
uint16_t latestErrorLen = 0;
uint16_t latestDebugLen = 0;

OSAP::OSAP(String _name) : Vertex("rt_" + _name){};

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
    case RT_DBG_STAT:
    case RT_DBG_ERRMSG:
    case RT_DBG_DBGMSG:
      // return w/ the res key & same issuing ID 
      payload[wptr ++] = PK_DEST;
      payload[wptr ++] = RT_DBG_RES;
      payload[wptr ++] = item->data[ptr + 3];
      // stash high water mark, errormsg count, debugmsgcount 
      ts_writeUint32(OSAP::loopItemsHighWaterMark, payload, &wptr);
      ts_writeUint32(errorCount, payload, &wptr);
      ts_writeUint32(debugCount, payload, &wptr);
      // optionally, a string... I know we switch() then if(), it's uggo, 
      if(item->data[ptr + 2] == RT_DBG_ERRMSG){
        ts_writeString(latestError, latestErrorLen, payload, &wptr, VT_SLOTSIZE / 2);
      } else if (item->data[ptr + 2] == RT_DBG_DBGMSG){
        ts_writeString(latestDebug, latestDebugLen, payload, &wptr, VT_SLOTSIZE / 2);
      }
      // that's the payload, I figure, 
      len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, wptr);
      stackClearSlot(item);
      stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
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