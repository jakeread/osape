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
#include "vport.h"
#include "../../drivers/indicators.h"
#include "../ucbus/ucbus_drop.h"

class VPort_UCBus_Drop : public VPort {
    public:
        VPort_UCBus_Drop();
        // startup / run 
        void init(void);
        void loop(void);
        uint8_t status(uint16_t rxAddr);
        // read, 
        void read(uint8_t **pck, pckm_t* pckm); 
        void clear(uint8_t location);
        // check 
        boolean cts(uint8_t rxAddr);
        // transmit 
        void send(uint8_t* pck, uint16_t pl, uint8_t rxAddr);
};

#endif 
#endif 