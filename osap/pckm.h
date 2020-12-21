/*
osap/pck.h

c struct for packet meta 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include <Arduino.h>

#include "vport.h"

#ifndef PCKM_H_
#define PCKM_H_

typedef struct {
    VPort* vpa;         // vport this arrived on 
    uint16_t len;       // full length, in bytes 
    unsigned long at;   // packet arrival time 
    uint8_t location;   // where in vport buffer / memory, for clearing 
    uint16_t rxAddr;     // when on the bus, where rx'd on (bus address, not vport indice)
    uint16_t txAddr;     // when on the bus, where tx'd from 
} pckm_t ; // packet meta 

#endif 