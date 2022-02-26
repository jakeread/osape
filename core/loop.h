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

#ifndef LOOP_H_
#define LOOP_H_ 

#include "vertex.h"

void recursor(Vertex* vt);
void handler(Vertex* vt);
void packetSwitch(Vertex* vt, uint8_t od, stackItem* item, uint16_t ptr, unsigned long now);

#endif 