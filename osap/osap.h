/*
osap/osap.h

virtual network node

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_H_
#define OSAP_H_

#include <arduino.h>
#include "ts.h"
#include "vport.h"
#include "../../drivers/indicators.h"
#include "../utils/cobs.h"

#define OSAP_MAX_VPORTS 16
#define RES_LENGTH 2048
#define OSAP_STALETIMEOUT 600
#define OSAP_TXKEEPALIVEINTERVAL 300

class OSAP {
private:
  // yonder ports,
  VPort* _vPorts[16];
  uint8_t _numVPorts = 0;
  // yither vmodules
  uint8_t _numVModules = 0;
  // dishing output, temp write buffer
  uint8_t _res[RES_LENGTH];
public:
  OSAP(String nodeName);
  // props
  String name;
  String description = "undescribed osap node";
  // fns
  boolean addVPort(VPort* vPort);
  void forward(uint8_t *pck, uint16_t pl, uint16_t ptr, VPort *vp, uint8_t vpi, uint8_t pwp);
  void write77(uint8_t *pck, VPort *vp);
  boolean formatResponseHeader(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, uint16_t checksum, VPort *vp, uint16_t vpi);
  void writeQueryDown(uint16_t *wptr);
  void writeEmpty(uint16_t *wptr);
  void readRequestVPort(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t rptr, uint16_t *wptr, uint16_t segsize, VPort* vp);
  void handleReadRequest(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, VPort* vp, uint16_t vpi, uint8_t pwp);
  void handlePingRequest(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, VPort* vp, uint16_t vpi, uint8_t pwp);
  void appReply(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, VPort* vp, uint16_t vpi, uint8_t *reply, uint16_t rl);
  void instructionSwitch(uint8_t *pck, uint16_t pl, uint16_t ptr, VPort *vp, uint8_t vpi, uint8_t pwp);
  void loop();
  // the handoff, 
  void handleAppPacket(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, VPort* vp, uint16_t vpi, uint8_t pwp);
};

#endif
