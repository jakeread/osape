/*
osap/vport_usbserial.cpp

virtual port, p2p

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vport_usbserial.h"

VPort_USBSerial::VPort_USBSerial():VPort("usb serial"){
  description = "vport wrap on arduino Serial object";
}

void VPort_USBSerial::init(void){
  // set frame lengths to zero,
  for(uint8_t i = 0; i < VPUSB_NUM_SPACES; i ++){
    _pl[i] = 0;
  }
  // start arduino serial object 
  Serial.begin(9600);
}

void VPort_USBSerial::loop(void){
  while(Serial.available()){
    _inBuffer[_bwp] = Serial.read();
    if(_inBuffer[_bwp] == 0){
      // end of COBS-encoded frame, 
      // count # of packets rx'd since we last transmitted, and re-assert open status 
      _rxSinceTx ++;
      _status = EP_PORTSTATUS_OPEN;
      // decode into packet slot, record length (to mark fullness) and record arrival time 
      size_t dcl = cobsDecode(_inBuffer, _bwp, _packet[_pwp]); 
      _pl[_pwp] = dcl;
      _packetArrivalTimes[_pwp] = millis(); // time this thing arrived 
      // reset byte write pointer, and find the next empty packet write space 
      _bwp = 0;
      boolean set = false;
      for(uint8_t i = 0; i <= VPUSB_NUM_SPACES; i ++){
        _pwp ++;
        if(_pwp >= VPUSB_NUM_SPACES){ _pwp = 0; }
        if(_pl[_pwp] == 0){ // if this frame-write-ptr hasn't been set to occupied,
          set = true;
          break; // this _pwp is next empty frame,
        }
      }
      if(!set){ sysError("no empty slot for serial read, protocol error!");}
    } else {
      _bwp ++;
    }
  }
}

uint8_t VPort_USBSerial::status(void){
  return _status;
}

void VPort_USBSerial::read(uint8_t **pck, uint16_t *pl, uint8_t *pwp, unsigned long* pat){
  uint8_t p = _lastPacket; // the last one we delivered,
  boolean retrieved = false;
  for(uint8_t i = 0; i <= VPUSB_NUM_SPACES; i ++){
    p ++;
    if(p >= VPUSB_NUM_SPACES) { p = 0; }
    if(_pl[p] > 0){ // this is an occupied packet, deliver that
      *pck = _packet[p]; // osap will look directly into our buffer ! 
      *pl = _pl[p]; // I *think* this is how we do this in c?
      *pwp = p; // packet write pointer ? the indice of the packet, to clear 
      *pat = _packetArrivalTimes[p];
      _lastPacket = p;
      retrieved = true;
      break;
    }
  }
  if(!retrieved){
    *pl = 0;
  }
}

// bus virtualf placeholder 
void VPort_USBSerial::read(uint8_t **pck, uint16_t *pl, uint8_t *pwp, unsigned long* pat, uint8_t* drop){
  *pl = 0;
  return;
}

void VPort_USBSerial::clear(uint8_t pwp){
  // frame consumed, clear to write-in,
  _pl[pwp] = 0;
  _packetArrivalTimes[pwp] = 0;
}

boolean VPort_USBSerial::cts(void){
  // no 77 yet, so 
  if(_status == EP_PORTSTATUS_OPEN){
    return true;
  } else {
    return false;
  }
}

// bus virtualf placeholder 
boolean VPort_USBSerial::cts(uint8_t drop){
  return false;
}

void VPort_USBSerial::send(uint8_t *pck, uint16_t pl){
  if(!cts()) return;
  size_t encLen = cobsEncode(pck, pl, _encodedOut);
  if(Serial.availableForWrite()){
    Serial.write(_encodedOut, encLen);
    Serial.flush();
  } else {
    sysError("NOT AVAILABLE");
  }
}

// bus virtualf placeholder 
void VPort_USBSerial::send(uint8_t *pck, uint16_t pl, uint8_t drop){
  return; 
}

/*
void OSAP::write77(uint8_t *pck, VPort *vp){
  uint16_t one = 1;
  pck[0] = PK_PPACK; // the '77'
  uint16_t bufSpace = vp->getBufSpace();
  ts_writeUint16(bufSpace, pck, &one);
  vp->lastRXBufferSpaceTransmitted = bufSpace;
  vp->rxSinceTx = 0;
}
*/

/*
  // loop for keepalive conditions, 
  for(uint8_t p = 0; p < _numVPorts; p ++){
    vp = _vPorts[p];
    // check if needs to tx keepalive, 
    uint16_t currentRXBufferSpace = vp->getBufSpace();
    if(currentRXBufferSpace > vp->lastRXBufferSpaceTransmitted || vp->lastTxTime + OSAP_TXKEEPALIVEINTERVAL < now){
      // has open space not reported, or needs to ping for port keepalive 
      if(vp->cts()){
        write77(_res, vp);
        vp->send(_res, 3);
      } 
    }
  } // end loop over ports (keepalive)
*/
