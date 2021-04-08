/*
osap/vt_ucBusHead.cpp

virtual port, bus head / host

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vt_ucBusHead.h"

#ifdef UCBUS_IS_HEAD

#include "../../drivers/indicators.h"
#include "../ucbus/ucbusHead.h"

// uuuuh, local thing and extern, not sure abt this
// file scoped object, global ptr
vertex_t _vt_ucBusHead;
vertex_t* vt_ucBusHead = &_vt_ucBusHead;

// locally, track which drop we shifted in a packet from last
uint8_t _lastDropHandled = 0;

// badness, should remove w/ direct copy in API eventually
uint8_t _tempBuffer[1024];

void vt_ucBusHead_setup(void) {
  _vt_ucBusHead.type = VT_TYPE_VBUS;
  _vt_ucBusHead.loop = &vt_ucBusHead_loop;
  _vt_ucBusHead.cts = &vt_ucBusHead_cts;
  _vt_ucBusHead.send = &vt_ucBusHead_send;
  stackReset(&_vt_ucBusHead);
  // start ucbus
  ucBusHead_setup();  // todo: rewrite as c object, not class
}

void vt_ucBusHead_loop() {
  // we need to shift items from the bus into the origin stack here
  // we can shift multiple in per turn, if stack space exists
  uint8_t drop = _lastDropHandled;
  for (uint8_t i = 1; i < UBH_DROP_OPS; i++) {
    drop++;
    if (drop >= UBH_DROP_OPS) {
      drop = 1;
    }
    if (ucBusHead_ctr(drop)) {
      // find a stack slot,
      uint8_t slot = 0;
      if (stackEmptySlot(&_vt_ucBusHead, VT_STACK_ORIGIN)) {
        // copy it in, 
        uint16_t len = ucBusHead_read(drop, _tempBuffer);
        stackLoadSlot(&_vt_ucBusHead, VT_STACK_ORIGIN, _tempBuffer, len);
      } else {
        // no more empty spaces this turn, continue 
        return; 
      }
    }
  }
}

boolean vt_ucBusHead_cts(uint8_t rxAddr) {
  // mapping rxAddr in osap space (where 0 is head) to ucbus drop-id space...
  return ucBusHead_ctsB(rxAddr);
}

void vt_ucBusHead_send(uint8_t* data, uint16_t len, uint8_t rxAddr) {
  if (rxAddr == 0) {
    sysError("attempt to busf from head to self");
    return;
  }
  ucBusHead_transmitB(data, len, rxAddr);
}

#endif