/*
osap/ts.cpp

typeset / keys / writing / reading

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "ts.h"

void ts_readBoolean(boolean* val, unsigned char* buf, uint16_t* ptr){
  if(buf[(*ptr) ++]){
    *val = true;
  } else {
    *val = false;
  }
}

void ts_writeBoolean(boolean val, unsigned char* buf, uint16_t* ptr){
  if(val){
    buf[(*ptr) ++] = 1;
  } else {
    buf[(*ptr) ++] = 0;
  }
}

void ts_writeInt16(int16_t val, unsigned char* buf, uint16_t* ptr){
  chunk_int16 chunk = { i: val };
  buf[(*ptr) ++] = chunk.bytes[0];
  buf[(*ptr) ++] = chunk.bytes[1];
}

void ts_writeInt32(int32_t val, unsigned char* buf, uint16_t* ptr){
  chunk_int32 chunk = { i: val };
  buf[(*ptr) ++] = chunk.bytes[0];
  buf[(*ptr) ++] = chunk.bytes[1];
  buf[(*ptr) ++] = chunk.bytes[2];
  buf[(*ptr) ++] = chunk.bytes[3];
}

void ts_readUint16(uint16_t* val, unsigned char* buf, uint16_t* ptr){
  *val = buf[(*ptr) + 1] << 8 | buf[(*ptr)];
  *ptr += 2;
}

void ts_writeUint8(uint8_t val, unsigned char* buf, uint16_t* ptr){
  buf[(*ptr) ++] = val;
}

void ts_writeUint16(uint16_t val, unsigned char* buf, uint16_t* ptr){
  buf[(*ptr) ++] = val & 255;
  buf[(*ptr) ++] = (val >> 8) & 255;
}

void ts_readUint32(uint32_t* val, unsigned char* buf, uint16_t* ptr){
  *val = buf[(*ptr) + 3] << 24 | buf[(*ptr) + 2] << 16 | buf[(*ptr) + 1] << 8 | buf[(*ptr)];
  *ptr += 4;
}

void ts_writeUint32(uint32_t val, unsigned char* buf, uint16_t* ptr){
  buf[(*ptr) ++] = val & 255;
  buf[(*ptr) ++] = (val >> 8) & 255;
  buf[(*ptr) ++] = (val >> 16) & 255;
  buf[(*ptr) ++] = (val >> 24) & 255;
}

void ts_writeFloat32(float val, volatile unsigned char* buf, uint16_t* ptr){
  chunk_float32 chunk;
  chunk.f = val;
  buf[(*ptr) ++] = chunk.bytes[0];
  buf[(*ptr) ++] = chunk.bytes[1];
  buf[(*ptr) ++] = chunk.bytes[2];
  buf[(*ptr) ++] = chunk.bytes[3];
}

float ts_readFloat32(unsigned char* buf, uint16_t* ptr){
  chunk_float32 chunk = { .bytes = { buf[(*ptr)], buf[(*ptr) + 1], buf[(*ptr) + 2], buf[(*ptr) + 3] } };
  (*ptr) += 4;
  return chunk.f;
}

void ts_writeFloat64(double val, volatile unsigned char* buf, uint16_t* ptr){
  chunk_float64 chunk;
  chunk.f = val;
  buf[(*ptr) ++] = chunk.bytes[0];
  buf[(*ptr) ++] = chunk.bytes[1];
  buf[(*ptr) ++] = chunk.bytes[2];
  buf[(*ptr) ++] = chunk.bytes[3];
  buf[(*ptr) ++] = chunk.bytes[4];
  buf[(*ptr) ++] = chunk.bytes[5];
  buf[(*ptr) ++] = chunk.bytes[6];
  buf[(*ptr) ++] = chunk.bytes[7];
}

void ts_writeString(String* val, unsigned char* buf, uint16_t* ptr){
  uint32_t len = val->length();
  buf[(*ptr) ++] = len & 255;
  buf[(*ptr) ++] = (len >> 8) & 255;
  buf[(*ptr) ++] = (len >> 16) & 255;
  buf[(*ptr) ++] = (len >> 24) & 255;
  val->getBytes(&buf[*ptr], len + 1);
  *ptr += len;
}

void ts_writeString(String val, unsigned char* buf, uint16_t* ptr){
  ts_writeString(&val, buf, ptr);
}

