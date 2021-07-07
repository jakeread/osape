/*
osap/endpoint.h

network : software interface

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef ENDPOINT_H_
#define ENDPOINT_H_

#include "vertex.h"

typedef struct vertex_t vertex_t;
typedef struct stackItem stackItem;

// endpoint handler responses must be one of these enum - 
enum EP_ONDATA_RESPONSES { EP_ONDATA_REJECT, EP_ONDATA_ACCEPT, EP_ONDATA_WAIT };

EP_ONDATA_RESPONSES endpointHandler(vertex_t* vt, uint8_t od, stackItem* item, uint16_t ptr);

struct endpoint_t {
  vertex_t* vt;
  // local data store & length, 
  uint8_t data[VT_SLOTSIZE];
  uint16_t dataLen = 0; 
  EP_ONDATA_RESPONSES (*onData)(uint8_t* data, uint16_t len) = nullptr;
  boolean (*beforeQuery)(void) = nullptr;
};

#endif 