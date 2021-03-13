/*
osap/vt_usbSerial.h

virtual port, p2p

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VPORT_USBSERIAL_H_
#define VPORT_USBSERIAL_H_

#include <arduino.h>
#include "vertex.h"

#define VPUSB_NUM_SPACES 16
#define VPUSB_SPACE_SIZE 512

// uuuuh classes are cancelled? 

void vt_usbSerial_setup(void);
void vt_usbSerial_loop(void);
boolean vt_usbSerial_cts(uint8_t drop);
void vt_usbSerial_send(uint8_t* data, uint16_t len, uint8_t rxAddr);
void vt_usbSerial_onOriginStackClear(uint8_t slot);

// tells linker that the thing exists & to... find it later?
// is declared in this cpp file 

extern vertex_t* vt_usbSerial;

#endif
