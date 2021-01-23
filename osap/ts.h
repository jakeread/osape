/*
osap/ts.h

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

#include <arduino.h>

// -------------------------------------------------------- Routing (Packet) Keys

#define PK_PPACK 77
#define PK_PTR 88
#define PK_DEST 99
#define PK_LLERR 44
#define PK_PORTF_KEY 11
#define PK_PORTF_INC 3
#define PK_BUSF_KEY 12
#define PK_BUSF_INC 5
#define PK_BUSB_KEY 14
#define PK_BUSB_INC 5

// -------------------------------------------------------- Destination Keys (arrival layer)

#define DK_APP 100 // application codes, go to -> main 
#define DK_PINGREQ 101  // ping request
#define DK_PINGRES 102  // ping reply
#define DK_EPREQ 103
#define DK_EPRES 104 
#define DK_RREQ 111     // read request
#define DK_RRES 112     // read response
#define DK_WREQ 113     // write request
#define DK_WRES 114     // write response
#define DK_LLBYTES 121  // routed byte escape 
#define DK_VMODULE 202          // datagram is obj for a vmodule: 4x 16b indices follow: from vm, obj / to vm, obj
#define DK_VMODULE_NACK 203     // your datagram recieved here but was occupied  
#define DK_VMODULE_YACK 204     // recieved and cleared at this endpoint 
#define DK_VMODULE_QUERY 212
#define DK_VMODULE_QUERY_ERR 213 
#define DK_VMODULE_QUERY_RES 214 

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

// -------------------------------------------------------- Application Keys 

#define AK_OK 100
#define AK_ERR 200 
#define AK_GOTOPOS 101           // goto pos 
#define AK_SETPOS 102            // set position to xyz 
#define AK_SETCURRENT 103       // set currents xyz 
#define AK_SETWAITTIME 104      // set queue wait time 
#define AK_SETRPM 105           // set spindle 
#define AK_QUERYMOVING 111          // is moving?
#define AK_QUERYPOS 112         // get current pos 
#define AK_QUERYQUEUELEN 113    // current queue len? 

#define AK_RUNCALIB 121
#define AK_READCALIB 122 
#define AK_SET_TC 123 

#define AK_BUSECHO 131

// -------------------------------------------------------- MVC Endpoints

#define EP_ERRKEY 150
#define EP_ERRKEY_QUERYDOWN 151
#define EP_ERRKEY_EMPTY 153
#define EP_ERRKEY_UNCLEAR 154

#define EP_NAME 171
#define EP_DESCRIPTION 172

#define EP_NUMVPORTS 181
#define EP_VPORT 182
#define EP_PORTTYPEKEY 183
#define EP_MAXSEGLENGTH 184
#define EP_PORTSTATUS 185
#define EP_MAXADDRESSES 186 

#define EP_PORTSTATUS_CLOSED 0
#define EP_PORTSTATUS_OPEN 1
#define EP_PORTSTATUS_CLOSING 2
#define EP_PORTSTATUS_OPENING 3 

#define EP_PORTTYPEKEY_DUPLEX 191
#define EP_PORTTYPEKEY_BUSHEAD 192
#define EP_PORTTYPEKEY_BUSDROP 193 

#define EP_NUMVMODULES 201
#define EP_VMODULE 202

#define EP_NUMINPUTS 211
#define EP_INPUT 212

#define EP_NUMOUTPUTS 221
#define EP_OUTPUT 222

#define EP_TYPE 231
#define EP_VALUE 232
#define EP_STATUS 233

#define EP_NUMROUES 243
#define EP_ROUTE 235

// ... etc, later

// -------------------------------------------------------- Chunks

union chunk_float32 {
  uint8_t bytes[4];
  float f;
};

union chunk_float64 {
  uint8_t bytes[8];
  double f;
};

union chunk_uint32 {
    uint8_t bytes[4];
    uint32_t u;
}; 

// -------------------------------------------------------- Reading and Writing

void ts_writeBoolean(boolean val, unsigned char* buf, uint16_t* ptr);

void ts_readUint16(uint16_t* val, uint8_t* buf, uint16_t* ptr);

void ts_writeUint16(uint16_t val, unsigned char* buf, uint16_t* ptr);

void ts_readUint32(uint32_t* val, unsigned char* buf, uint16_t* ptr);

void ts_writeUint32(uint32_t val, unsigned char* buf, uint16_t* ptr);

void ts_writeFloat32(float val, volatile unsigned char* buf, uint16_t* ptr);

void ts_writeFloat64(double val, volatile unsigned char* buf, uint16_t* ptr);

void ts_writeString(String val, unsigned char* buf, uint16_t* ptr);

#endif 