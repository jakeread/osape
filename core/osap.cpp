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
#include "../utils/cobs.h"

OSAP::OSAP(String _name) : Vertex(_name){};

void OSAP::loop(void){
  // this is the root, so we kick all of the internal net operation from here 
  osapLoop(this);
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
  debugPrint(msg);
}

void OSAP::debug(String msg, OSAPDebugStreams stream){
  debugPrint(msg);
}