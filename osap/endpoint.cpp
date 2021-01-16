/*
osap/endpoint.cpp

data element / osap software runtime api 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "endpoint.h"

Endpoint::Endpoint(boolean (*fp)(uint8_t* data, uint16_t len)){
    onNewData = fp;
}

void Endpoint::fill(uint8_t* pck, uint16_t ptr, pckm_t* pckm, uint16_t txVM, uint16_t txEP){
    // copy the full packet to our local store, 
    // TODO: should also align with 'write' ... 
    // at the moment, they are disconnected and that's broken 
    memcpy(pckStore, pck, pckm->len);
    dataStart = ptr;
    dataLen = pckm->checksum - 9;
    // this is true now, 
    dataNew = true; 
    // copy all meta information in, 
    pckmStore.vpa = pckm->vpa;
    pckmStore.len = pckm->len;
    pckmStore.at = pckm->at;
    pckmStore.location = pckm->location;
    pckmStore.rxAddr = pckm->rxAddr;
    pckmStore.txAddr = pckm->txAddr;
    pckmStore.segSize = pckm->segSize;
    pckmStore.checksum = pckm->checksum;
    // incl. this turnaround meta 
    rxFromVM = txVM;
    rxFromEP = txEP;
}

boolean Endpoint::consume(){
    return onNewData(&(pckStore[dataStart]), dataLen);
}

void Endpoint::write(uint8_t* data, uint16_t len){
    memcpy(dataStore, data, len);
    dataStoreLen = len;
}