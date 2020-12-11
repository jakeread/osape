/*
osap/vport.cpp

virtual port, p2p

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vport.h"

VPort::VPort(String vPortName){
  name = vPortName;
}

void VPort::setRecipRxBufSpace(uint16_t len){
  _recipRxBufSpace = len;
}

void VPort::decrimentRecipBufSpace(void){
  if(_recipRxBufSpace < 1){
    _recipRxBufSpace = 0;
  } else {
    _recipRxBufSpace --;
  }
  lastTxTime = millis();
}

boolean VPort::cts(void){
  if(_recipRxBufSpace > 0 && status){
    return true;
  } else {
    return false;
  }
}
