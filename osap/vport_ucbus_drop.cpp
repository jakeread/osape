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
    ucBusDrop->init(false, 14); // for now, this here?
}

void VPort_UCBus_Drop::loop(void){
    // this is all interrupts 
}

uint8_t VPort_UCBus_Drop::status(void){
    // should be polymorphic, per drop, but not implemented 
    return EP_PORTSTATUS_OPEN;
}

// placeholder 
void VPort_UCBus_Drop::read(uint8_t** pck, uint16_t* pl, uint8_t* pwp, unsigned long* pat){
    *pl = 0;
    return;
}

// busses 
void VPort_UCBus_Drop::read(uint8_t** pck, uint16_t* pl, uint8_t* pwp, unsigned long* pat, uint8_t* drop){
    ucBusDrop->read_b_ptr(pck, pat);
    *pwp = 0;  // pwp just == drop rx'd on for now... single size buffer 
    *drop = 0; // drops always recieve from the head 
    return;
}

void VPort_UCBus_Drop::clear(uint8_t pwp){
    ucBusDrop->clear_b_ptr(); // pwp is meaningless, should really amend this code mess (sorry) 
}

// placeholder 
boolean VPort_UCBus_Drop::cts(void){
    return false;
}

boolean VPort_UCBus_Drop::cts(uint8_t drop){
    if(drop == 0 && ucBusDrop->ctr_b()){
        return true;
    } else {
        return false;
    }
}

// placeholder 
void VPort_UCBus_Drop::send(uint8_t* pck, uint16_t pl){
    return;
}

void VPort_UCBus_Drop::send(uint8_t* pck, uint16_t pl, uint8_t drop){
    ucBusDrop->transmit(pck, pl);
}

#endif 