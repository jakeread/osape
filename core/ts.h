/*
core/ts.h

typeset / keys / writing / reading

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef TS_H_
#define TS_H_

#include <Arduino.h>

// -------------------------------------------------------- Vertex Type Keys
// will likely use these in the netrunner: 

#define VT_TYPE_ROOT 22       // top level 
#define VT_TYPE_MODULE 23     // collection of things (?) or something, idk yet 
#define VT_TYPE_ENDPOINT 24   // software endpoint w/ read/write semantics 
#define VT_TYPE_CODE 25       // autonomous graph dwellers 
#define VT_TYPE_VPORT 44      // virtual ports 
#define VT_TYPE_VBUS 45       // maybe bus-drop / bus-head / bus-cohost are differentiated 

// -------------------------------------------------------- TIMES

#define TIMES_STALE_MSG 800

// -------------------------------------------------------- Routing (Packet) Keys

#define PK_PTR 88
#define PK_DEST 99
#define PK_SIB_KEY 15 
#define PK_SIB_INC 3 
#define PK_PARENT_KEY 16 
#define PK_PARENT_INC 3 
#define PK_CHILD_KEY 14 
#define PK_CHILD_INC 3 
#define PK_PFWD_KEY 11 
#define PK_PFWD_INC 1 
#define PK_BFWD_KEY 12
#define PK_BFWD_INC 3
#define PK_SCOPE_REQ_KEY 21
#define PK_SCOPE_REQ_INC 1
#define PK_SCOPE_RES_KEY 22 
#define PK_SCOPE_RES_INC 1
#define PK_LLESCAPE_KEY 44
#define PK_LLESCAPE_INC 1

// -------------------------------------------------------- Endpoint Keys 

#define EP_SS_ACK 101       // the ack 
#define EP_SS_ACKLESS 121   // single segment, no ack 
#define EP_SS_ACKED 122     // single segment, request ack 
#define EP_QUERY 131        // query request 
#define EP_QUERY_RESP 132   // reply to query request 

// -------------------------------------------------------- BUS ACTION KEYS (outside OSAP scope)

#define UB_AK_SETPOS 102
#define UB_AK_GOTOPOS 105 

// -------------------------------------------------------- Type Keys 

#define TK_BOOL     2

#define TK_UINT8    4
#define TK_INT8     5
#define TK_UINT16   6
#define TK_INT16    7
#define TK_UINT32   8
#define TK_INT32    9
#define TK_UINT64   10
#define TK_INT64    11

#define TK_FLOAT16  24
#define TK_FLOAT32  26
#define TK_FLOAT64  28

// -------------------------------------------------------- Chunks

union chunk_float32 {
  uint8_t bytes[4];
  float f;
};

union chunk_float64 {
  uint8_t bytes[8];
  double f;
};

union chunk_int16 {
  uint8_t bytes[2];
  int16_t i;
};

union chunk_int32 {
  uint8_t bytes[4];
  int32_t i;
};

union chunk_uint32 {
    uint8_t bytes[4];
    uint32_t u;
}; 

// -------------------------------------------------------- Reading and Writing

void ts_readBoolean(boolean* val, unsigned char* buf, uint16_t* ptr);

void ts_writeBoolean(boolean val, unsigned char* buf, uint16_t* ptr);

void ts_writeInt16(int16_t val, unsigned char* buf, uint16_t* ptr);

void ts_writeInt32(int32_t val, unsigned char* buf, uint16_t* ptr);

void ts_readUint16(uint16_t* val, uint8_t* buf, uint16_t* ptr);

void ts_writeUint8(uint8_t val, unsigned char* buf, uint16_t* ptr);

void ts_writeUint16(uint16_t val, unsigned char* buf, uint16_t* ptr);

void ts_readUint32(uint32_t* val, unsigned char* buf, uint16_t* ptr);

void ts_writeUint32(uint32_t val, unsigned char* buf, uint16_t* ptr);

void ts_writeFloat32(float val, volatile unsigned char* buf, uint16_t* ptr);

void ts_writeFloat64(double val, volatile unsigned char* buf, uint16_t* ptr);

void ts_writeString(String* val, unsigned char* buf, uint16_t* ptr);
void ts_writeString(String val, unsigned char* buf, uint16_t* ptr);

#endif 