/*
osap/vt_usbSerial.cpp

serial port, virtualized
does single-ended flowcontrol (from pc -> here) 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vt_usbSerial.h"
#include "../../drivers/indicators.h"
#include "ts.h"
#include "../utils/cobs.h"
#include "../utils/syserror.h"

vertex_t _vt_usbSerial;
vertex_t* vt_usbSerial = &_vt_usbSerial;

// incoming 
uint8_t _inBuffer[VPUSB_SPACE_SIZE];
uint8_t _pwp = 0; // packet to write into 
uint16_t _bwp = 0; // byte to write at 

uint8_t _inHold[VPUSB_NUM_SPACES][VPUSB_SPACE_SIZE];
uint16_t _inLengths[VPUSB_NUM_SPACES];

// acks left to transmit 
uint8_t _acksAwaiting = 0;

// outgoing
uint8_t _encodedOut[VPUSB_SPACE_SIZE];

void usbSerialSetup(void){
  //vt_usbSerial = &vt;
  // configure self, 
  _vt_usbSerial.type = VT_TYPE_VPORT;
  _vt_usbSerial.name = "usbSerial";
  _vt_usbSerial.loop = &usbSerialLoop;
  _vt_usbSerial.cts = &usbSerialCTS;
  _vt_usbSerial.send = &usbSerialSend;
  // configure spaces, 
  for(uint8_t i = 0; i < VPUSB_NUM_SPACES; i ++){
    _inLengths[i] = 0;
  }
  // start arduino serial object 
  Serial.begin(9600);
}

void usbSerialLoop(void){
  // ack if necessary (if didn't tx ack out on reciprocal send last)
  if(_acksAwaiting){
    usbSerialSend(_encodedOut, 0, 0);
  }
  // find stack space for old messages, 
  uint8_t space = 0;
  if(vertexSpace(&_vt_usbSerial, &space)){
    for(uint8_t h = 0; h < VPUSB_NUM_SPACES; h ++){
      if(_inLengths[h] > 0){
        memcpy(_vt_usbSerial.stack[space], _inHold[h], _inLengths[h]);
        _inLengths[h] = 0;
        _acksAwaiting ++;
      }
    }
  }
  // then check about new messages: 
  while(Serial.available()){
    _inBuffer[_bwp] = Serial.read();
    if(_inBuffer[_bwp] == 0){
      // end of COBS-encoded frame, 
      // decode into packet slot, record length (to mark fullness) and record arrival time 
      // unless we are w/o packet spaces, so check:
      uint8_t slot = 0;
      if(vertexSpace(&_vt_usbSerial, &slot)){
        // decode into the vertex stack 
        // cobsDecode returns the length of the decoded packet
        uint16_t len = cobsDecode(_inBuffer, _bwp, _vt_usbSerial.stack[slot]); 
        _vt_usbSerial.stackLen[slot] = len;
        //sysError("serial wrote " + String(len) + " to " + String(slot));
        // record time of arrival, 
        _vt_usbSerial.stackArrivalTimes[slot] = millis();
        // reset byte write pointer, and find the next empty packet write space 
        _bwp = 0;
        // ack it, 
        _acksAwaiting ++;
      } else {
        // push into local rx stack, 
        boolean held = false;
        for(uint8_t h = 0; h < VPUSB_NUM_SPACES; h ++){
          if(_inLengths[h] == 0){
            uint16_t len = cobsDecode(_inBuffer, _bwp, _inHold[h]);
            _inLengths[h] = len;
            held = true;
            break;
          }
        }
        if(!held){ // dropped, 
          sysError("! serial no space !");
          _bwp = 0;
        }
      }      
    } else {
      _bwp ++;
    }
  }
}

boolean usbSerialCTS(uint8_t drop){
  return true;
}

uint8_t _shift[VPUSB_SPACE_SIZE];

void usbSerialSend(uint8_t* data, uint16_t len, uint8_t rxAddr){
  // damn, this is not fast 
  _shift[0] = _acksAwaiting;
  _acksAwaiting = 0;
  memcpy(&(_shift[1]), data, len);
  size_t encLen = cobsEncode(_shift, len + 1, _encodedOut);
  if(Serial.availableForWrite()){
    Serial.write(_encodedOut, encLen);
    Serial.flush();
  } else {
    sysError("on write, serial not available");
  }
}

/*

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

// rxAddr just here for busses, 
uint8_t VPort_USBSerial::status(uint16_t rxAddr){
  // check transitions 
  if(_status == EP_PORTSTATUS_CLOSED){
    if(Serial.availableForWrite()) _status = EP_PORTSTATUS_OPEN;
  } else {
    if(!Serial.availableForWrite()) _status = EP_PORTSTATUS_CLOSED;
  }
  return _status;
}

void VPort_USBSerial::read(uint8_t **pck, pckm_t* pckm){
  uint8_t p = _lastPacket; // the last one we delivered,
  boolean retrieved = false;
  for(uint8_t i = 0; i <= VPUSB_NUM_SPACES; i ++){
    p ++;
    if(p >= VPUSB_NUM_SPACES) { p = 0; }
    if(_pl[p] > 0){ // this is an occupied packet, deliver that
      *pck = _packet[p]; // osap will look directly into our buffer ! 
      pckm->vpa = this;
      pckm->len = _pl[p]; // I *think* this is how we do this in c?
      pckm->at = _packetArrivalTimes[p];
      pckm->location = p;
      _lastPacket = p;
      retrieved = true;
      break;
    }
  }
  if(!retrieved){
    pckm->len = 0;
  }
}

void VPort_USBSerial::clear(uint8_t location){
  // frame consumed, clear to write-in,
  _pl[location] = 0;
  _packetArrivalTimes[location] = 0;
}

// ignore rxAddr: that's interface for busses
boolean VPort_USBSerial::cts(uint8_t rxAddr){
  // no 77 yet, so 
  if(status(0) == EP_PORTSTATUS_OPEN){
    return true;
  } else {
    return false;
  }
}

*/



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
