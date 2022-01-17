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

// endpoints have *routes* which they tx to... 

#define ENDPOINT_MAX_ROUTES 4

enum EP_ROUTE_MODES { EP_ROUTE_ACKLESS, EP_ROUTE_ACKED };
enum EP_ROUTE_STATES { EP_TX_IDLE, EP_TX_FRESH, EP_TX_AWAITING_ACK, EP_TX_AWAITING_AND_FRESH };

struct endpoint_route_t {
  uint8_t path[64];
  uint8_t pathLen = 0;
  uint8_t ackId = 0;    // this'll limit the # of simultaneously tx'd-to routes to 255, considering each needs unique ackid 
  EP_ROUTE_STATES state = EP_TX_IDLE;
  unsigned long txTime = 0;
  unsigned long timeoutLength = 1000; // ms 
  // properties likely to be exposed to mvc 
  EP_ROUTE_MODES ackMode = EP_ROUTE_ACKLESS;
  uint16_t segSize = 256;
};

// endpoint handler responses must be one of these enum - 

enum EP_ONDATA_RESPONSES { EP_ONDATA_REJECT, EP_ONDATA_ACCEPT, EP_ONDATA_WAIT };

struct endpoint_t {
  vertex_t* vt;
  // local data store & length, 
  uint8_t data[VT_SLOTSIZE];
  uint16_t dataLen = 0; 
  // callbacks: on new data & before a query is written out 
  EP_ONDATA_RESPONSES (*onData)(uint8_t* data, uint16_t len) = nullptr;
  boolean (*beforeQuery)(void) = nullptr;
  // routes, for tx-ing to:
  endpoint_route_t routes[ENDPOINT_MAX_ROUTES];
  uint16_t numRoutes = 0;
  uint16_t lastRouteServiced = 0;
  uint8_t nextAckId = 77;
};

// route adder: 
// vertex_t* ep is a mistake: osapBuildEndpoint is broken, and returns a vertex... 
// we *should* have a better cpp API for this, but don't, that's next go-round 
boolean endpointAddRoute(endpoint_t* ep, uint8_t* path, uint16_t pathLen, EP_ROUTE_MODES mode);

// endpoint writer... 
void endpointWrite(endpoint_t* ep, uint8_t* data, uint16_t len);

// endpoint check-tx-state-machine 
void endpointLoop(endpoint_t* ep, unsigned long now);

// endpoint api-to-check-all-clear:
boolean endpointAllClear(endpoint_t* ep);

// a master handler: 
EP_ONDATA_RESPONSES endpointHandler(endpoint_t* ep, uint8_t od, stackItem* item, uint16_t ptr);

#endif 