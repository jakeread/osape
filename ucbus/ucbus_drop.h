/*
osap/drivers/ucbus_head.h

beginnings of a uart-based clock / bus combo protocol

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#ifndef UCBUS_DROP_H_
#define UCBUS_DROP_H_

#include "../../config.h"

#ifdef UCBUS_IS_DROP

#include <arduino.h>

#include "../../drivers/indicators.h"
#include "dip_ucbus_config.h"
#include "../utils/peripheral_nums.h"
#include "../utils/syserror.h"

#define UBD_SER_USART SERCOM1->USART
#define UBD_SERCOM_CLK SERCOM1_GCLK_ID_CORE
#define UBD_GCLKNUM_PICK 7
#define UBD_COMPORT PORT->Group[0]
#define UBD_TXPIN 16  // x-0
#define UBD_TXBM (uint32_t)(1 << UBD_TXPIN)
#define UBD_RXPIN 18  // x-2
#define UBD_RXBM (uint32_t)(1 << UBD_RXPIN)
#define UBD_RXPO 2
#define UBD_TXPERIPHERAL PERIPHERAL_C
#define UBD_RXPERIPHERAL PERIPHERAL_C

// baud bb baud
// 63019 for a very safe 115200
// 54351 for a go-karting 512000
// 43690 for a trotting pace of 1MHz
// 21845 for the E30 2MHz
// 0 for max-speed 3MHz
#define UBD_BAUD_VAL 0

#define UBD_DE_PIN 19 // driver output enable: set HI to enable, LO to tri-state the driver 
#define UBD_DE_BM (uint32_t)(1 << UBD_DE_PIN)
#define UBD_DE_PORT PORT->Group[0] 
#define UBD_DRIVER_ENABLE UBD_DE_PORT.OUTSET.reg = UBD_DE_BM
#define UBD_DRIVER_DISABLE UBD_DE_PORT.OUTCLR.reg = UBD_DE_BM
#define UBD_RE_PIN 19 // receiver output enable, set LO to enable the RO, set HI to tri-state RO 
#define UBD_RE_BM (uint32_t)(1 << UBD_RE_PIN)
#define UBD_RE_PORT PORT->Group[0]
#define UBD_TE_PIN 17  // termination enable, drive LO to enable to internal termination resistor, HI to disable
#define UBD_TE_BM (uint32_t)(1 << UBD_TE_PIN)
#define UBD_TE_PORT PORT->Group[0]

#define UBD_BUFSIZE 1024

#define UBD_CLOCKRESET 15

#define UB_AK_GOTOPOS 91
#define UB_AK_SETPOS  92
#define UB_AK_SETRPM  93

class UCBus_Drop {
   private:
    // singleton-ness 
		static UCBus_Drop* instance;
    // timing, 
    volatile uint64_t timeBlink = 0;
    uint16_t blinkTime = 10000;
    // input buffer & word 
    volatile uint8_t inWord[2];
    volatile uint8_t inHeader;
    volatile uint8_t inByte;
    volatile boolean lastWordAHadToken = false;
    uint8_t inBufferA[UBD_BUFSIZE];
    volatile uint16_t inBufferARp = 0;
    volatile uint16_t inBufferALen = 0; // writes at terminal zero, 
    volatile boolean lastWordBHadToken = false;
    uint8_t inBufferB[UBD_BUFSIZE];
    volatile uint16_t inBufferBRp = 0;
    volatile uint16_t inBufferBLen = 0;
    // output, 
    volatile uint8_t outWord[2];
    volatile uint8_t outHeader;
    volatile uint8_t outByte;
    uint8_t outBuffer[UBD_BUFSIZE];
    volatile uint16_t outBufferRp = 0;
    volatile uint16_t outBufferLen = 0;
    const uint8_t headerMask = 0b00111111;
    const uint8_t dropIdMask = 0b00001111; 
    const uint8_t tokenWord = 0b00100000;
    const uint8_t noTokenWord = 0b00000000;
   public:
    UCBus_Drop();
		static UCBus_Drop* getInstance(void);
    // available time count, 
    volatile uint16_t timeTick = 0;
    // isrs 
    void rxISR(void);
    void dreISR(void);
    void txcISR(void);
    // handlers 
    void onRxISR(void);
    void onPacketARx(void);
    //void onPacketBRx(void);
    // our physical bus address, 
    volatile uint8_t id = 0;
    // the api, eh 
    void init(boolean useDipPick, uint8_t ID);
		boolean ctr_a(void);  // return true if RX complete / buffer ready 
    boolean ctr_b(void);
    size_t read_a(uint8_t *dest);  // ship les bytos
    size_t read_b(uint8_t *dest);
    boolean cts(void); // true if tx buffer empty, 
    void transmit(uint8_t *data, uint16_t len);
};

extern UCBus_Drop* ucBusDrop;

#endif
#endif 