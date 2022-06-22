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

#if 0 

#include "../core/vertex.h"

// ---------------------------------------------- Routes

enum EP_ROUTE_STATES { EP_TX_IDLE, EP_TX_FRESH, EP_TX_AWAITING_ACK, EP_TX_AWAITING_AND_FRESH };

class EndpointRoute : Route {
  public: 
    uint8_t ackId = 0;
    uint8_t ackMode = EP_ROUTEMODE_ACKLESS;
    EP_ROUTE_STATES state = EP_TX_IDLE;
    unsigned long lastTxTime = 0;
    // constructor, 
    EndpointRoute(uint8_t _mode);
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
    #warning TODO: should override destHandler()... 
    // methods,
    void write(uint8_t* _data, uint16_t len);
    void addRoute(EndpointRoute* _route);
    boolean clearToWrite(void);
    // routes, for tx-ing to:
    EndpointRoute* routes[ENDPOINT_MAX_ROUTES];
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

// ---------------------------------------------- Runtimes 

// loop over all endpoints, 
void endpointMainLoop(void);

// loop per-endpoint, 
void endpointLoop(Endpoint* ep, unsigned long now);

// a master handler: 
EP_ONDATA_RESPONSES endpointHandler(Endpoint* ep, stackItem* item, uint16_t ptr);

#endif 

#endif 