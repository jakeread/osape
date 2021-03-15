/*
osap/vport_ucbus_drop.h

virtual port, bus drop, ucbus 

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

#ifdef UCBUS_IS_DROP

#include <Arduino.h>
#include "vertex.h"

void vt_ucBusDrop_setup(void);
void vt_ucBusDrop_loop(void);
boolean vt_ucBusDrop_cts(uint8_t rxAddr);
void vt_ucBusDrop_send(uint8_t* data, uint16_t len, uint8_t rxAddr);

extern vertex_t* vt_ucBusDrop;

#endif 
#endif 