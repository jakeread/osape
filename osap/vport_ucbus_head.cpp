/*
osap/vport_ucbus_head.cpp

virtual port, bus head / host 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vport_ucbus_head.h"

#ifdef UCBUS_IS_HEAD

VPort_UCBus_Head::VPort_UCBus_Head():VPort("ucbus head"){
    // set props here, if defined in .h, they inherit from parent ! 
    description = "vport wrap on ucbus head";
    portTypeKey = EP_PORTTYPEKEY_BUSHEAD;
    maxSegLength = UBH_BUFSIZE - 2; // (1) is drop id, (2) is rcrxb
    maxAddresses = 15;  // 0 for the head, drops 0-14 map 1-15 
}

void VPort_UCBus_Head::init(void){
    ucBusHead->init();
}

void VPort_UCBus_Head::loop(void){
    // use timer to trigger ucBusHead->timerISR() directly 
}

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

boolean VPort_UCBus_Head::cts(uint8_t rxAddr){
    if(rxAddr == 0){
        sysError("attempt to read cts of self");
        return false;
    }
    return ucBusHead->cts_b(rxAddr - 1);
}

void VPort_UCBus_Head::send(uint8_t* pck, uint16_t pl, uint8_t rxAddr){
    // logical address is drop + 1: 0th 'drop' is the head in address space. 
    // this might be a bugfarm, but the bit is extra address space, so it lives... 
    if(rxAddr == 0){
        sysError("attempt to busf from head to self");
        return;
    }
    ucBusHead->transmit_b(pck, pl, rxAddr - 1);
}

#endif 