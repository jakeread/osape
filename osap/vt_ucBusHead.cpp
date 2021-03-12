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
#include "../ucbus/ucbus_head.h"

// uuuuh, local thing and extern, not sure abt this 
// file scoped object, global ptr 
vertex_t _vt_ucBusHead;
vertex_t* vt_ucBusHead = &_vt_ucBusHead;

void ucbusHeadSetup(void){
    _vt_ucBusHead.type = VT_TYPE_VBUS;
    _vt_ucBusHead.name = "ucbus head";
    _vt_ucBusHead.loop = &ucbusHeadLoop;
    _vt_ucBusHead.cts = &ucbusHeadCTS;
    _vt_ucBusHead.send = &ucbusHeadSend;
    // start ucbus 
    ucBusHead->init(); // todo: rewrite as c object, not class 
}

void ucbusHeadLoop(void){
    // noop, I don't think, should all happen on the ucbus timer, 
    // though we need to load from ucbus -> this stack, for reading 
}

boolean ucbusHeadCTS(uint8_t rxAddr){
    return ucBusHead->cts_b(rxAddr - 1);
}

void ucbusHeadSend(uint8_t* data, uint16_t len, uint8_t rxAddr){
    // logical address is drop + 1: 0th 'drop' is the head in address space. 
    // this might be a bugfarm, but the bit is extra address space, so it lives... 
    if(rxAddr == 0){
        sysError("attempt to busf from head to self");
        return;
    }
    ucBusHead->transmit_b(data, len, rxAddr - 1);
}

/*

uint8_t VPort_UCBus_Head::status(uint16_t rxAddr){
    // use time of last byte arrival from this drop, 
    if(ucBusHead->lastrc[rxAddr - 1] + 500 < millis()){
        return EP_PORTSTATUS_CLOSED;
    } else {
        return EP_PORTSTATUS_OPEN;
    }
}

void VPort_UCBus_Head::read(uint8_t** pck, pckm_t* pckm){
    // track last-drop-dished, increment thru to find next w/ occupied space 
    uint8_t dr = 0; // drop recieved: ubh->readPtr() fills this in, 
    unsigned long pat = 0;
    pckm->vpa = this;
    pckm->len = ucBusHead->readPtr(&dr, pck, &pat);
    pckm->at = pat; // arrival time 
    pckm->location = dr + 1; // quick hack, at the moment pwp is just the drop ... see note in clear 
    pckm->txAddr = dr + 1; // addr that tx'd this packet 
    pckm->rxAddr = 0; // us, who rx'd it
    return;
}

void VPort_UCBus_Head::clear(uint8_t location){
    // eventually, should be drop, pwp: if we ever buffer more than 1 space in each drop space
    ucBusHead->clearPtr(location - 1);
}
*/

#endif 