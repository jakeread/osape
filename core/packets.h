/*
osap/packets.h

reading / writing from osap packets / datagrams 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_PACKETS_H_
#define OSAP_PACKETS_H_

#include <Arduino.h>
#include "vertex.h"

boolean findPtr(uint8_t* pck, uint16_t* ptr);
boolean walkPtr(uint8_t* pck, Vertex* vt, uint8_t steps, uint16_t ptr = 4);
uint16_t writeDatagram(uint8_t* gram, uint16_t maxGramLength, Route* route, uint8_t* payload, uint16_t payloadLen);
uint16_t writeReply(uint8_t* ogGram, uint8_t* gram, uint16_t maxGramLength, uint8_t* payload, uint16_t payloadLen);

#endif 