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
}

void VPort_UCBus_Head::init(void){
    ucBusHead->init();
}

void VPort_UCBus_Head::loop(void){
    // use timer to trigger ucBusHead->timerISR() directly 
}

uint8_t VPort_UCBus_Head::status(void){
    // should be polymorphic 
    return EP_PORTSTATUS_OPEN;
}

// placeholder, virtualf, duplex 
void VPort_UCBus_Head::read(uint8_t** pck, uint16_t* pl, uint8_t* pwp, unsigned long* pat){
    *pl = 0;
    return;
}

void VPort_UCBus_Head::read(uint8_t** pck, uint16_t* pl, uint8_t* pwp, unsigned long* pat, uint8_t* drop){
    // track last-drop-dished, increment thru to find next w/ occupied space 
    uint8_t dr = 0; // drop recieved 
    *pl = ucBusHead->readPtr(&dr, pck, pat);
    *drop = dr;
    *pwp = dr; // quick hack, at the moment pwp is just the drop ... see note in clear 
    return;
}

void VPort_UCBus_Head::clear(uint8_t pwp){
    ucBusHead->clearPtr(pwp); // eventually, should be drop, pwp: if we ever buffer more than 1 space in each drop space
}

// placeholder, virtualf, duplex 
boolean VPort_UCBus_Head::cts(void){
    return false;
}

boolean VPort_UCBus_Head::cts(uint8_t drop){
    return ucBusHead->cts_b(drop);
}

// placeholder, virtualf, duplex 
void VPort_UCBus_Head::send(uint8_t* pck, uint16_t pl){
    return;
}

void VPort_UCBus_Head::send(uint8_t* pck, uint16_t pl, uint8_t drop){
    ucBusHead->transmit_b(pck, pl, drop);
}

#endif 