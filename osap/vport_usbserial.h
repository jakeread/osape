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
  uint8_t _inBuffer[VPUSB_SPACE_SIZE];
  uint8_t _packet[VPUSB_NUM_SPACES][VPUSB_SPACE_SIZE]; 
  volatile uint16_t _pl[VPUSB_NUM_SPACES];
  unsigned long _packetArrivalTimes[VPUSB_NUM_SPACES];
  uint8_t _pwp = 0; // packet write pointer,
  uint16_t _bwp = 0; // byte write pointer,
  uint8_t _lastPacket = 0; // last packet we delivered to osap: tracks round-robin packet handling 
  // outgoing cobs-copy-in,
  uint8_t _encodedOut[VPUSB_SPACE_SIZE];
  // tracking / keepalive
  uint16_t _lastRXBufferSpaceTransmitted = 0; 
  uint16_t _rxSinceTx = 0;
  unsigned long _lastTxTime = 0;
  // internal status 
  uint8_t _status = EP_PORTSTATUS_CLOSED;
public:
  VPort_USBSerial();
  uint8_t portTypeKey = EP_PORTTYPEKEY_DUPLEX;
  uint16_t maxSegLength = VPUSB_SPACE_SIZE - 6;
  void init(void);
  void loop(void);
  uint8_t status(void);
  void read(uint8_t** pck, pckm_t* pckm);
  void clear(uint8_t location);
  boolean cts(uint8_t rxAddr); // for bus, placeholder, not a pro with virtual fns 
  void send(uint8_t *pck, uint16_t pl, uint8_t rxAddr); // rxAddr ignored, 
};

#endif
