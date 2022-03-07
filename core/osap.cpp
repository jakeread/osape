/*
osap/osap.cpp

osap root / vertex factory

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#include "osap.h"
#include "loop.h"
#ifdef OSAP_DEBUG
#include "./osap_debug.h"
#endif 

OSAP::OSAP(String _name) : Vertex(_name){};

void OSAP::loop(void){
  // this is the root, so we kick all of the internal net operation from here 
  loopRecursor(this);
  // run setups, 
  setupRecursor(this);
  // run transfers, 
  transferRecursor(this);
}