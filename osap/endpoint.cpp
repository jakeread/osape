/*
osap/endpoint.cpp

data element / osap software runtime api 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "endpoint.h"

Endpoint::Endpoint(boolean (*fp)(uint8_t* data, uint16_t len)){
    onNewData = fp;
}