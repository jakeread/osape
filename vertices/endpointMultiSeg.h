/*
osap/vertices/endpointMultiSeg.h

network : software interface for big bois 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2022

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef ENDPOINTMULTISEG_H_
#define ENDPOINTMULTISEG_H_

#include "../core/vertex.h"
#include "endpoint.h"

class EndpointMultiSeg : public Vertex {
  public:
    void* dataPtr;
    uint32_t dataLen;
    // override vertex loop, 
    void loop(void) override;
  // constructor 
  EndpointMultiSeg(Vertex* _parent, String _name, void* _dataPtr, uint32_t _dataLen);
};

#endif 