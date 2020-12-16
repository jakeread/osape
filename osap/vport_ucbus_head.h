/*
osap/vport_ucbus_had.h

virtual port, bus head, ucbus 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2020

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VPORT_UCBUS_HEAD_H_
#define VPORT_UCBUS_HEAD_H_ 

#include "../../config.h"

#ifdef UCBUS_IS_HEAD

#include <Arduino.h>
#include "vport.h"
#include "../../drivers/indicators.h"
#include "../ucbus/ucbus_head.h"

class VPort_UCBus_Head : public VPort {
    private:
    public:
        VPort_UCBus_Head();
        uint8_t portTypeKey = EP_PORTTYPEKEY_BUSHEAD;
        uint16_t maxSegLength = UBH_BUFSIZE;
        // basics
        void init(void);
        void loop(void);
        // to handle frames 
        // fills in the packet, it's length, write pointer (to clear), and packet arrival time 
        void getPacket(uint8_t** pck, uint16_t* pl, uint8_t* pwp, unsigned long* pat);
        void clearPacket(uint8_t pwp);
        // info 
        uint16_t getBufSize(void); // 'spaces' count
        uint16_t getBufSpace(void);// num of spaces unoccupied 
        // transmit packet, length, 
        void sendPacket(uint8_t *pck, uint16_t pl, uint8_t drop);
};

#endif
#endif 