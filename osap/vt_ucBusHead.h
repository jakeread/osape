/*
osap/vt_ucBusHead.h

virtual port, bus head, ucbus 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2020

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VPORT_UCBUS_HEAD_H_
#define VPORT_UCBUS_HEAD_H_ 

#include "../../config.h"

#ifdef UCBUS_IS_HEAD

#include <Arduino.h>
#include "vertex.h"

void ucbusHeadSetup(void);
void ucbusHeadLoop(void);
boolean ucbusHeadCTS(uint8_t rxAddr);
void ucbusHeadSend(uint8_t* data, uint16_t len, uint8_t rxAddr);

extern vertex_t* vt_ucBusHead;

#endif
#endif 