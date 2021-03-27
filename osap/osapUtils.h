/*
osap/osapUtils.h

common routines 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_UTILS_H_
#define OSAP_UTILS_H_

#include <Arduino.h>

boolean ptrLoop(uint8_t* pck, uint16_t* ptr);
boolean reverseRoute(uint8_t* pck, uint16_t rptr, uint8_t* repl, uint16_t* replyPtr);

#endif 