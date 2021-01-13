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

class Endpoint{
    public:
    Endpoint(boolean (*fp)(uint8_t* data, uint16_t len));
    boolean (*onNewData)(uint8_t* data, uint16_t len);
};

#endif 