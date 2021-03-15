/*
osap/vport_ucbus_drop.cpp

virtual port, bus drop, ucbus 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2020

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vt_ucBusDrop.h"

#ifdef UCBUS_IS_DROP

#include "../../drivers/indicators.h"
#include "../ucbus/ucBusDrop.h"

vertex_t _vt_ucBusDrop;
vertex_t* vt_ucBusDrop = &_vt_ucBusDrop;

// badness, direct write in future 
uint8_t _tempBuffer[1024];

void vt_ucBusDrop_setup(void){
    _vt_ucBusDrop.type = VT_TYPE_VBUS;
    _vt_ucBusDrop.name = "ucbus drop";
    _vt_ucBusDrop.loop = &vt_ucBusDrop_loop;
    _vt_ucBusDrop.cts = &vt_ucBusDrop_cts;
    _vt_ucBusDrop.send = &vt_ucBusDrop_send;
    // start it: use DIP 
    ucBusDrop_setup(true, 0);
}

void vt_ucBusDrop_loop(void){
    // will want to shift(?) from ucbus inbuffer to vertex origin stack 
    if(ucBusDrop_ctrB()){
      // find a slot, 
      uint8_t slot = 0;
      if(stackEmptySlot(&_vt_ucBusDrop, VT_STACK_ORIGIN)){
        // copy in to origin stack 
        uint16_t len = ucBusDrop_readB(_tempBuffer);
        stackLoadSlot(&_vt_ucBusDrop, VT_STACK_ORIGIN, _tempBuffer, len);
      } else {
        // no empty space, will wait in bus 
      }
    }
}

boolean vt_ucBusDrop_cts(uint8_t rxAddr){
  // immediately clear? & transmit only to head 
  if(rxAddr == 0 && ucBusDrop_ctsB()){
    return true;
  } else {
    return false;
  }
}

void vt_ucBusDrop_send(uint8_t* data, uint16_t len, uint8_t rxAddr){
  // can't tx not-to-the-head, will drop pck 
  if(rxAddr != 0) return;
  // if the bus is ready, drop it,
  if(ucBusDrop_ctsB()){
    ucBusDrop_transmitB(data, len);
  } else {
    sysError("ubd tx while not clear"); // should be a check immediately beforehand ...  
  }
}

/*
void VPort_UCBus_Drop::read(uint8_t** pck, pckm_t* pckm){
    unsigned long pat = 0;
    pckm->vpa = this;
    pckm->len = ucBusDrop->read_b_ptr(pck, &pat);
    pckm->at = pat;
    pckm->location = 0;
    pckm->txAddr = 0; // the head transmitted to us, 
    pckm->rxAddr = ucBusDrop->id + 1;
    return;
}
*/

/*
// used to have a vertex local outgoing buffer, this can be the vertex's destination buffer afaiak
void VPort_UCBus_Drop::init(void){
    ucBusDrop->init(true, 0); 
    for(uint8_t i = 0; i < UBD_OUTBUFFER_COUNT; i ++){
        _outBufferLen[i] = 0;
    }
}

void VPort_UCBus_Drop::loop(void){
    // check our transmit buffer,
    if(ucBusDrop->cts()){
        for(uint8_t i = 0; i < UBD_OUTBUFFER_COUNT; i ++){
            if(_outBufferLen[i] > 0){
                ucBusDrop->transmit(_outBuffer[i], _outBufferLen[i]);
                _outBufferLen[i] = 0;
            }
        }
    }
}
*/

#endif 