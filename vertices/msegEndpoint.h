/*
osap/vertices/msegEndpoint.h

network : software interface for multisegment endpoints 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2022

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef MSEGENDPOINT_H_
#define MSEGENDPOINT_H_

#include "../core/vertex.h"

struct msegEndpoint_t {
    vertex_t* vt;
    // underlying data... ptr to,
    uint8_t* dataPtr;
    uint16_t dataLen;
    // data is going to read / write w/o user alerts, so that might be it... 
    
};

msegEndpoint_t* osapBuildMsegEndpoing(String name, uint8_t* dptr, uint16_t len);

// main ops, 
void msegEndpointMainLoop(void);

// loops per 
void msegEndpointLoop(msegEndpoint_t* msep, unsigned long now);

// ... handler breakout 
void msegHandler(msegEndpoint_t* msep, stackItem* item, uint16_t ptr);

#endif 