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
#include "ts.h"

boolean findPtr(uint8_t* pck, uint16_t* ptr);
uint16_t writeDatagram(uint8_t* gram, uint16_t maxGramLength, Route* route, uint8_t* payload, uint16_t payloadLen);
uint16_t writeReply(uint8_t* ogGram, uint8_t* gram, uint16_t maxGramLength, uint8_t* payload, uint16_t payloadLen);

#endif 