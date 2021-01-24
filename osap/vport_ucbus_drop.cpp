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

#include "vport_ucbus_drop.h"

#ifdef UCBUS_IS_DROP

VPort_UCBus_Drop::VPort_UCBus_Drop():VPort("ucbus drop"){
    description = "vport wrap on ucbus drop";
    portTypeKey = EP_PORTTYPEKEY_BUSDROP;
    maxSegLength = UBD_BUFSIZE - 2; // 1 for rcrxb, 1 for drop id (on return) 
    maxAddresses = 15;  // 0 for head, 0-14 map 1-15 for drops 
}

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

uint8_t VPort_UCBus_Drop::status(uint16_t rxAddr){
    // ignore rxAddr: hopefully no-one is querying for status of other drops, we cannot know 
    // could check if this is 0, but... when busses become more advanced and we want this, will write 
    // could also check, if req. is for rxAddr 0, if time of last byte from the head is > some interval 
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
    // immediately clear?
    if(rxAddr == 0 && ucBusDrop->cts()){
        return true;
    }
    // if not, check for empty space in outbuffer, 
    for(uint8_t i = 0; i < UBD_OUTBUFFER_COUNT; i ++){
        if(_outBufferLen[i] == 0){
            return true;
        }
    }
    // if not, not cts 
    return false;
}

void VPort_UCBus_Drop::send(uint8_t* pck, uint16_t pl, uint8_t rxAddr){
    // can't tx not-to-the-head 
    if(rxAddr != 0) return;
    // if the bus is ready, drop it,
    if(ucBusDrop->cts()){
        ucBusDrop->transmit(pck, pl);
    } else {
        // otherwise, stash in the store... 
        // if no spaces, game over, no graceful save, things have gone poorly 
        for(uint8_t i = 0; i < UBD_OUTBUFFER_COUNT; i ++){
            if(_outBufferLen[i] == 0){
                memcpy(_outBuffer[i], pck, pl);
                _outBufferLen[i] = pl;
                return;
            }
        }
    }
}

#endif 