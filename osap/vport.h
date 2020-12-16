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
private:
  uint16_t _recipRxBufSpace = 1;
public:
  VPort(String vPortName);
  String name;
  String description = "undescribed vport";
  uint8_t portTypeKey = EP_PORTTYPEKEY_DUPLEX;
  uint16_t maxSegLength = 0;
  virtual void init(void) = 0;
  virtual void loop(void) = 0;
  // keepalive log 
  uint16_t lastRXBufferSpaceTransmitted = 0;
  uint16_t rxSinceTx = 0; // debugging: count packets received since last spaces txd 
  unsigned long lastTxTime = 0;
  // handling incoming frames,
  virtual void getPacket(uint8_t** pck, uint16_t* pl, uint8_t* pwp, unsigned long* pat) = 0;
  // *be sure* that getPacket sets pl to zero if no packet emerges, 
  // consider making boolean return, true if packet? 
  virtual void clearPacket(uint8_t pwp) = 0;
  virtual uint16_t getBufSpace(void) = 0;
  virtual uint16_t getBufSize(void) = 0;
  // dish outgoing frames, and check if open to send them?
  uint8_t status = EP_PORTSTATUS_CLOSED; // open / closed-ness -> OSAP can set, VP can set. 
  virtual boolean cts(void); // is a connection established & is the reciprocal buffer nonzero?
  virtual void sendPacket(uint8_t* pck, uint16_t pl) = 0; // take this frame, copying out of the buffer I pass you
  // internal state,
  void setRecipRxBufSpace(uint16_t len);
  void decrimentRecipBufSpace(void);
};

#endif
