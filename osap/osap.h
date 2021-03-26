/*
osap/osap.h

osap root / vertex factory 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_H_
#define OSAP_H_

#include "vertex.h"
#include "endpoint.h"

// vertex factory & root vertex 

extern vertex_t* osap;

void osapSetup(void);
void osapLoop(void);

boolean osapAddVertex(vertex_t* vertex);
vertex_t* osapBuildEndpoint(
    String name, 
    boolean (*onData)(uint8_t* data, uint16_t ptr, uint16_t len), 
    boolean (*beforeQuery)(void)
);

#endif 