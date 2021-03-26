/*
osap/osapLoop.h

main osap op: whips data vertex-to-vertex 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_LOOP_H_
#define OSAP_LOOP_H_ 

#include "vertex.h"

void osapRecursor(vertex_t* vt);
void osapHandler(vertex_t* vt);
void osapSwitch(vertex_t* vt, uint8_t od, stackItem* item, uint16_t ptr, unsigned long now);
boolean ptrLoop(uint8_t* pck, uint16_t* ptr);
boolean reverseRoute(uint8_t* pck, uint16_t rptr, uint8_t* repl, uint16_t* replyPtr);

#endif 