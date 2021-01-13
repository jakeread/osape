/*
osap/osap.h

virtual network node

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_H_
#define OSAP_H_

#include <arduino.h>
#include "ts.h"
#include "vport.h"
#include "endpoint.h"
#include "../../drivers/indicators.h"
#include "../utils/cobs.h"

#define OSAP_MAX_VPORTS 16
#define OSAP_MAX_ENDPOINTS 32 
#define RES_LENGTH 2048
#define OUT_LENGTH 2048
#define OSAP_STALETIMEOUT 600
#define OSAP_TXKEEPALIVEINTERVAL 300

class OSAP {
private:
  // yonder ports,
  VPort* _vPorts[OSAP_MAX_VPORTS];
  uint8_t _numVPorts = 0;
  // yither vmodules / endpoints 
  uint8_t _numVModules = 0;
  Endpoint* _endpoints[OSAP_MAX_ENDPOINTS];
  uint16_t _numEndpoints = 0;
  // dishing output, temp write buffer
  uint8_t _res[RES_LENGTH];
  // dishing packets, temp write buffer 
  uint8_t _tx[OUT_LENGTH];
public:
  OSAP(String nodeName);
  // props
  String name;
  String description = "undescribed osap node";
  // fns
  boolean addVPort(VPort* vPort);
  Endpoint* endpoint(boolean (*onNewData)(uint8_t* data, uint16_t len));
  // tx 
  boolean send(uint8_t* txroute, uint16_t routelen, uint16_t segsize, uint8_t* data, uint16_t datalen);
  // reply to a packet, 
  boolean formatResponseHeader(uint8_t* pck, uint16_t ptr, pckm_t* pckm, uint16_t reslen);
  void appReply(uint8_t* pck, pckm_t* pckm, uint8_t* reply, uint16_t repLen);
  // replies to mvc-requests 
  void writeQueryDown(uint16_t* wptr);
  void writeEmpty(uint16_t* wptr);
  void readRequestVPort(uint8_t* pck, uint16_t ptr, pckm_t* pckm, uint16_t rptr, uint16_t* wptr, VPort* vp);
  void handleReadRequest(uint8_t* pck, uint16_t ptr, pckm_t* pckm);
  void handleEntryPointRequest(uint8_t* pck, uint16_t ptr, pckm_t* pckm);
  void handlePingRequest(uint8_t* pck, uint16_t ptr, pckm_t* pckm);
  // main loop,
  void portforward(uint8_t* pck, uint16_t ptr, pckm_t* pckm, VPort* fwvp);
  void busforward(uint8_t* pck, uint16_t ptr, pckm_t* pckm, VPort* fwvp);
  void forward(uint8_t* pck, uint16_t ptr, pckm_t* pckm);
  void instructionSwitch(uint8_t* pck, uint16_t ptr, pckm_t* pckm);
  void loop();
  // the handoff, 
  void handleAppPacket(uint8_t* pck, uint16_t ptr, pckm_t* pckm);
  void handleVModulePacket(uint8_t* pck, uint16_t ptr, pckm_t* pckm);
};

#endif
