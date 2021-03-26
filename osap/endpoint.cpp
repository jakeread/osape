/*
osap/endpoint.cpp

network : software interface

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "endpoint.h"
#include "../../drivers/indicators.h"

// handle, return true to clear out. false to wait one turn 
boolean endpointHandler(vertex_t* vt, uint8_t od, stackItem* item, uint16_t ptr){
    DEBUG3PIN_TOGGLE;
    return true; 
}