/*
osap/vport.h

virtual port, p2p

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VPORT_H_
#define VPORT_H_

#include <arduino.h>
#include "ts.h"
#include "./osape/utils/syserror.h"

class VPort {
public:
  // constructor 
  VPort(String vPortName);
  // properties 
  String name = "unnamed vport";
  String description = "undescribed vport";
  uint8_t portTypeKey = EP_PORTTYPEKEY_DUPLEX;
  uint16_t maxSegLength = 0;
  // startup 
  virtual void init(void) = 0;  
  // runtime 
  virtual void loop(void) = 0;
  // status: 0: closed, 1: open, 2: closing, 3: opening (defines in ts.h)
  // TODO: make status polymorphic for busses, status(uint8_t busAddr);
  virtual uint8_t status(void) = 0;
  // give OSAP the data (set pl = 0 if no data)
  virtual void read(uint8_t** pck, uint16_t* pl, uint8_t* pwp, unsigned long* pat) = 0; // duplex type 
  virtual void read(uint8_t** pck, uint16_t* pl, uint8_t* pwp, unsigned long* pat, uint8_t drop) = 0; // bus type 
  // this packet can be deleted, has been forwarded / dealt with 
  virtual void clear(uint8_t pwp) = 0;
  // clear to send? backpressure OK and port open?
  virtual boolean cts(void) = 0;
  virtual boolean cts(uint8_t drop) = 0;
  // transmit these bytes, 
  virtual void send(uint8_t* pck, uint16_t pl) = 0;
  virtual void send(uint8_t* pck, uint16_t pl, uint8_t drop) = 0;
};

#endif
