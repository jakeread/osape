/*
osap/endpoint.h

data element / osap software runtime api 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef ENDPOINT_H_
#define ENDPOINT_H_

#include <Arduino.h>
#include "ts.h"
#include "vport.h"

class Endpoint{
    public:
    // self
    Endpoint(boolean (*pfp)(uint8_t* data, uint16_t len));
    Endpoint(boolean (*pfp)(uint8_t* data, uint16_t len), boolean (*qfp)(void));
    boolean (*onNewData)(uint8_t* data, uint16_t len) = 0;
    boolean (*onQuery)(void) = 0;
    uint16_t indice = 0;
    // endpoint stores data, as well as full packet from whence data arrived 
    void fill(uint8_t* pck, uint16_t ptr, pckm_t* pckm, uint16_t txVM, uint16_t txEP);
    // try to consume local data (at software API)
    boolean consume(void);
    // write new data 
    void write(uint8_t* data, uint16_t len);
    // the local buffer... when these are typed, can be smarter about memory:
    uint8_t dataStore[2048];
    uint16_t dataStoreLen = 0;
    // the consumption buffer: store entire packet
    uint8_t pckStore[2048];
    pckm_t pckmStore;
    uint16_t rxFromVM;
    uint16_t rxFromEP;
    uint16_t dataStart = 0;     // indice where endpoint bytes begin 
    uint16_t dataLen = 0;       // size of data 
    boolean dataNew = false;
};

#endif 