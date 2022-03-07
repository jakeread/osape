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

// largely semantic class, OSAP represents the root vertex in whichever context 
// and it's where run the main loop from, etc... 

class OSAP : public Vertex {
  public: 
    void loop(void) override;
    OSAP(String _name);// : Vertex(_name);
};

#endif 