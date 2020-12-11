/*
osap/vport_usbserial.h

virtual port, p2p

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VPORT_USBSERIAL_H_
#define VPORT_USBSERIAL_H_

#include <arduino.h>
#include "vport.h"
#include "../../drivers/indicators.h"

#define VPUSB_NUM_SPACES 64
#define VPUSB_SPACE_SIZE 1028

class VPort_USBSerial : public VPort {
private:
  // unfortunately, looks like we need to write-in to temp,
  // and decode out of that
  uint8_t _encodedPacket[VPUSB_NUM_SPACES][VPUSB_SPACE_SIZE];
  uint8_t _packet[VPUSB_NUM_SPACES][VPUSB_SPACE_SIZE];
  volatile uint16_t _pl[VPUSB_NUM_SPACES];
  unsigned long _packetArrivalTimes[VPUSB_NUM_SPACES];
  uint8_t _pwp = 0; // packet write pointer,
  uint16_t _bwp = 0; // byte write pointer,
  uint8_t _lastPacket = 0; // last packet written into
  // outgoing cobs-copy-in,
  uint8_t _encodedOut[VPUSB_SPACE_SIZE];
  // this is just for debug,
  uint8_t _ringPacket[VPUSB_SPACE_SIZE];
public:
  VPort_USBSerial();
  // props
  uint16_t maxSegLength = VPUSB_SPACE_SIZE - 6;
  // code
  void init(void);
  void loop(void);
  // handle incoming frames
  void getPacket(uint8_t** pck, uint16_t* pl, uint8_t* pwp, unsigned long* pat);
  void clearPacket(uint8_t pwp);
  uint16_t getBufSize(void);
  uint16_t getBufSpace(void);
  // dish outgoing frames, and check if cts
  void sendPacket(uint8_t *pck, uint16_t pl);
};

#endif
