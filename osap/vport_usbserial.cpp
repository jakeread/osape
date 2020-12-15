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
  Serial.begin(9600);
}

void VPort_USBSerial::loop(void){
  while(Serial.available()){
    _encodedPacket[_pwp][_bwp] = Serial.read();
    if(_encodedPacket[_pwp][_bwp] == 0){
      rxSinceTx ++;
      // sysError(String(getBufSpace()) + " " + String(_bwp));
      // indicate we recv'd zero
      // CLKLIGHT_TOGGLE;
      // decode from rx-ing frame to interface frame,
      status = true; // re-assert open whenever received packet incoming 
      size_t dcl = cobsDecode(_encodedPacket[_pwp], _bwp, _packet[_pwp]);
      _pl[_pwp] = dcl; // this frame now available, has this length,
      _packetArrivalTimes[_pwp] = millis(); // time this thing arrived 
      // reset byte write pointer
      _bwp = 0;
      // find next empty frame, that's new frame write pointer
      boolean set = false;
      for(uint8_t i = 0; i <= VPUSB_NUM_SPACES; i ++){
        _pwp ++;
        if(_pwp >= VPUSB_NUM_SPACES){ _pwp = 0; }
        if(_pl[_pwp] == 0){ // if this frame-write-ptr hasn't been set to occupied,
          set = true;
          break; // this _pwp is next empty frame,
        }
      }
      if(!set){
        sysError("no empty slot for serial read, protocol error!");
        uint16_t apparentSpace = getBufSpace();
        sysError("reads: " + String(apparentSpace));
        sysError("last txd recip: " + String(lastRXBufferSpaceTransmitted));
        sysError("rxd since last tx: " + String(rxSinceTx));
        sysError(String(_pl[0]));
        sysError(String(_pl[1]));
        sysError(String(_pl[2]));
        sysError(String(_pl[3]));
        sysError(String(_pl[4]));
        sysError(String(_pl[5]));
        sysError(String(_pl[6]));
        sysError(String(_pl[7]));
        sysError(String(_pl[8]));
        delay(5000);
      }
    } else {
      _bwp ++;
    }
  }
}

void VPort_USBSerial::getPacket(uint8_t **pck, uint16_t *pl, uint8_t *pwp, unsigned long* pat){
  uint8_t p = _lastPacket; // the last one we delivered,
  boolean retrieved = false;
  for(uint8_t i = 0; i <= VPUSB_NUM_SPACES; i ++){
    p ++;
    if(p >= VPUSB_NUM_SPACES) { p = 0; }
    if(_pl[p] > 0){ // this is an occupied packet, deliver that
      *pck = _packet[p]; // same, is this passing the ptr, yeah?
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

void VPort_USBSerial::clearPacket(uint8_t pwp){
  // frame consumed, clear to write-in,
  //sysError("clear " + String(pwp));
  _pl[pwp] = 0;
  _packetArrivalTimes[pwp] = 0;
}

uint16_t VPort_USBSerial::getBufSize(void){
  return VPUSB_NUM_SPACES;
}

uint16_t VPort_USBSerial::getBufSpace(void){
  uint16_t sum = 0;
  // any zero-length frame is not full,
  for(uint16_t i = 0; i < VPUSB_NUM_SPACES; i++){
    if(_pl[i] == 0){
      sum ++;
    }
  }
  // but one is being written into,
  //if(_bwp > 0){
  sum --;
  //}
  // if we're very full this might wrap / invert, so
  if(sum > VPUSB_NUM_SPACES){
    sum = 0;
  }
  // arrivaderci
  return sum;
}

void VPort_USBSerial::sendPacket(uint8_t *pck, uint16_t pl){
  size_t encLen = cobsEncode(pck, pl, _encodedOut);
  if(Serial.availableForWrite()){
    //DEBUG1PIN_ON;
    Serial.write(_encodedOut, encLen);
    Serial.flush();
    //DEBUG1PIN_OFF;
  } else {
    sysError("NOT AVAILABLE");
  }
}
