/*
osap/vertices/endpoint.h

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

#include "../core/vertex.h"

// ---------------------------------------------- Routes

enum EP_ROUTE_MODES { EP_ROUTE_ACKLESS, EP_ROUTE_ACKED };
enum EP_ROUTE_STATES { EP_TX_IDLE, EP_TX_FRESH, EP_TX_AWAITING_ACK, EP_TX_AWAITING_AND_FRESH };

struct endpointRoute_t {
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

// ---------------------------------------------- Endpoints 

// endpoint handler responses must be one of these enum - 
enum EP_ONDATA_RESPONSES { EP_ONDATA_REJECT, EP_ONDATA_ACCEPT, EP_ONDATA_WAIT };

// default handlers, 
EP_ONDATA_RESPONSES onDataDefault(uint8_t* data, uint16_t len);
boolean beforeQueryDefault(void);

class Endpoint : public Vertex {
  public:
    // local data store & length, 
    uint8_t data[VT_SLOTSIZE];
    uint16_t dataLen = 0; 
    // callbacks: on new data & before a query is written out 
    EP_ONDATA_RESPONSES (*onData_cb)(uint8_t* data, uint16_t len) = onDataDefault;
    boolean (*beforeQuery_cb)(void) = beforeQueryDefault;
    // we override vertex loop, 
    void loop(void) override;
    // methods,
    void write(uint8_t* _data, uint16_t len);
    void addRoute(uint8_t* path, uint16_t pathLen, EP_ROUTE_MODES mode);
    boolean clearToWrite(void);
    // routes, for tx-ing to:
    endpointRoute_t routes[ENDPOINT_MAX_ROUTES];
    uint16_t numRoutes = 0;
    uint16_t lastRouteServiced = 0;
    uint8_t nextAckId = 77;
    // base constructor, 
    Endpoint(   
      Vertex* _parent, String _name, 
      EP_ONDATA_RESPONSES (*_onData)(uint8_t* data, uint16_t len),
      boolean (*_beforeQuery)(void)
    );
    // these are called "delegating constructors" ... best reference is 
    // here: https://en.cppreference.com/w/cpp/language/constructor 
    // onData only, 
    Endpoint(   
      Vertex* _parent, String _name,
      EP_ONDATA_RESPONSES (*_onData)(uint8_t* data, uint16_t len)
    ) : Endpoint ( 
      _parent, _name, _onData, nullptr
    ){};
    // beforeQuery only, 
    Endpoint(   
      Vertex* _parent, String _name, 
      boolean (*_beforeQuery)(void)
    ) : Endpoint (
      _parent, _name, nullptr, _beforeQuery
    ){};
    // name only, 
    Endpoint(   
      Vertex* _parent, String _name
    ) : Endpoint (
      _parent, _name, nullptr, nullptr
    ){};
};

// ---------------------------------------------- Endpoint Route / Write API 

// endpoint writer... 
void endpointWrite(Endpoint* ep, uint8_t* data, uint16_t len);

// route adder: 
boolean endpointAddRoute(Endpoint* ep, uint8_t* path, uint16_t pathLen, EP_ROUTE_MODES mode);

// endpoint api-to-check-all-clear:
boolean endpointAllClear(Endpoint* ep);

// ---------------------------------------------- Runtimes 

// loop over all endpoints, 
void endpointMainLoop(void);

// loop per-endpoint, 
void endpointLoop(Endpoint* ep, unsigned long now);

// a master handler: 
EP_ONDATA_RESPONSES endpointHandler(Endpoint* ep, stackItem* item, uint16_t ptr);

#endif 