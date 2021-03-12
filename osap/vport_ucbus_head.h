/*
osap/vport_ucbus_head.h

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

extern vertex_t* vt_ucbusHead;

/*
class VPort_UCBus_Head : public VPort {
    public:
        VPort_UCBus_Head();
        // startup / runtime 
        void init(void);
        void loop(void);
        uint8_t status(uint16_t rxAddr); // this polymorphic also? or just always open ATM, lose 'em into the abyss? 
        // read ... from each drop ? 
        // might have to count # drops also, no? 
        // bus-drops just report 1 drop: the head 
        void read(uint8_t** pck, pckm_t* pckm);
        void clear(uint8_t location);
        // cts(uint8_t drop); !
        boolean cts(uint8_t rxAddr);
        // don't know how to do virtual overriding etc 
        // send(pck, pl, drop);
        void send(uint8_t *pck, uint16_t pl, uint8_t rxAddr);
};
*/

#endif
#endif 