/*
osap/vport_ucbus_drop.cpp

virtual port, bus drop, ucbus 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2020

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vport_ucbus_drop.h"

#ifdef UCBUS_IS_DROP

VPort_UCBus_Drop::VPort_UCBus_Drop():VPort("ucbus drop"){
    description = "vport wrap on ucbus drop";
    portTypeKey = EP_PORTTYPEKEY_BUSDROP;
    maxSegLength = UBD_BUFSIZE - 2; // 1 for rcrxb, 1 for drop id (on return) 
}

void VPort_UCBus_Drop::init(void){
    #warning needs to init w/ args...
    // id == 6, so rxAddr = 7 
    ucBusDrop->init(false, 6); // for now, this here?
}

void VPort_UCBus_Drop::loop(void){
    // this is all interrupts 
}

uint8_t VPort_UCBus_Drop::status(void){
    // should be polymorphic, per drop, but not implemented 
    return EP_PORTSTATUS_OPEN;
}

// busses 
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

void VPort_UCBus_Drop::clear(uint8_t pwp){
    ucBusDrop->clear_b_ptr(); // pwp is meaningless, should really amend this code mess
}

boolean VPort_UCBus_Drop::cts(uint8_t rxAddr){
    if(rxAddr == 0 && ucBusDrop->ctr_b()){
        return true;
    } else {
        return false;
    }
}

void VPort_UCBus_Drop::send(uint8_t* pck, uint16_t pl, uint8_t rxAddr){
    if(!cts(rxAddr)) return;
    ucBusDrop->transmit(pck, pl);
}

#endif 