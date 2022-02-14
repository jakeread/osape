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

// vertex factory & root vertex 

extern vertex_t* osap;

void osapSetup(String name);
void osapMainLoop(void);

boolean osapAddVertex(vertex_t* vertex);

#endif 