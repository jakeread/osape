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

#ifndef UCBUS_HEAD_H_
#define UCBUS_HEAD_H_

#include "../../config.h"

#ifdef UCBUS_IS_HEAD

#include <arduino.h>

#include "../../drivers/indicators.h"
#include "../utils/peripheral_nums.h"
#include "../utils/syserror.h"
#include "../utils/clocks_d51.h"

#define TIMER_A_GCLK_NUM 9
#define TIMER_B_GCLK_NUM 10

#define UBH_SER_USART SERCOM1->USART
#define UBH_SERCOM_CLK SERCOM1_GCLK_ID_CORE
#define UBH_GCLKNUM_PICK 7
#define UBH_COMPORT PORT->Group[0]
#define UBH_TXPIN 16  // x-0
#define UBH_TXBM (uint32_t)(1 << UBH_TXPIN)
#define UBH_RXPIN 18  // x-2
#define UBH_RXBM (uint32_t)(1 << UBH_RXPIN)
#define UBH_RXPO 2 // RX on SER-2
#define UBH_TXPERIPHERAL PERIPHERAL_C
#define UBH_RXPERIPHERAL PERIPHERAL_C

// baud bb baud
// 63019 for a very safe 115200
// 54351 for a go-karting 512000
// 43690 for a trotting pace of 1MHz
// 21845 for the E30 2MHz
// 0 for max-speed 3MHz
#define UBH_BAUD_VAL 0

#define UBH_DE_PIN 16 // driver output enable: set HI to enable, LO to tri-state the driver 
#define UBH_DE_BM (uint32_t)(1 << UBH_DE_PIN)
#define UBH_DE_PORT PORT->Group[1] 
#define UBH_RE_PIN 19 // receiver output enable, set LO to enable the RO, set HI to tri-state RO 
#define UBH_RE_BM (uint32_t)(1 << UBH_RE_PIN)
#define UBH_RE_PORT PORT->Group[0]
#define UBH_TE_PIN 17  // termination enable, drive LO to enable to internal termination resistor, HI to disable
#define UBH_TE_BM (uint32_t)(1 << UBH_TE_PIN)
#define UBH_TE_PORT PORT->Group[0]

#define UBH_BUFSIZE 1024

// # of drops to look for 
// count 15: 2^4 bits, with one case reserved for the clock reset 
#define UBH_DROP_OPS 14

#define UB_AK_GOTOPOS 91
#define UB_AK_SETPOS  92
#define UB_AK_SETRPM  93

// PLEASE NOTE: this requires a 100kHz tick, use interrupt timer, 
// fire the timerISR there. 

class UCBus_Head {
   private:
    // singleton-ness 
		static UCBus_Head* instance;
    // input is big for the head, 
    volatile uint8_t inWord[2];
    volatile uint8_t inHeader;
    volatile uint8_t inByte;
    uint8_t inBuffer[UBH_DROP_OPS][UBH_BUFSIZE];  // per-drop incoming bytes 
    volatile uint16_t inBufferWp[UBH_DROP_OPS];   // per-drop incoming write pointer
    volatile uint16_t inBufferLen[UBH_DROP_OPS];  // per-drop incoming bytes, len of, set when EOP detected
    volatile boolean inLastHadToken[UBH_DROP_OPS];
    // transmit buffers for A / B Channels 
    uint8_t outBufferA[UBH_BUFSIZE];
    volatile uint16_t outBufferARp = 0;
    volatile uint16_t outBufferALen = 0;
    uint8_t outBufferB[UBH_BUFSIZE];
    volatile uint16_t outBufferBRp = 0;
    volatile uint16_t outBufferBLen = 0;
    // doublet 
    volatile uint8_t outWord[2];
    volatile uint8_t currentDropTap = 0; // drop we are currently 'txing' to / drop that will reply on this cycle
    volatile uint8_t outHeader;
    volatile uint8_t outByte;
    const uint8_t headerMask =    0b00111111;
    const uint8_t dropIdMask =    0b00001111; 
                                  // 0b00|token|channel|4bit id
    const uint8_t tokenWordA =    0b00100000; // CHA, data byte present 
    const uint8_t noTokenWordA =  0b00000000; // CHA, data byte not present 
    const uint8_t tokenWordB =    0b00110000; // CHB, data byte present 
    const uint8_t noTokenWordB =  0b00010000; // CHB, data byte not present 
    volatile uint8_t lastSpareEOP = 0;        // last channel we transmitted spare end-of-packet on
    // uart 
    void startupUART(void);
   public:
    UCBus_Head();
		static UCBus_Head* getInstance(void);
    // isrs 
    void timerISR(void);
    void rxISR(void);
    void txISR(void);
    // reciprocal recieve buffer spaces 
    volatile uint8_t rcrxb[UBH_DROP_OPS];
    // handles 
    void init(void);
    boolean ctr(uint8_t drop); // is there ahn packet to read at this drop 
    size_t read(uint8_t drop, uint8_t *dest);  // get 'them bytes fam 
		boolean cts_a(void);  // return true if TX complete / buffer ready
    boolean cts_b(uint8_t drop);
    void transmit_a(uint8_t *data, uint16_t len);  // ship bytes: broadcast to all 
    void transmit_b(uint8_t *data, uint16_t len, uint8_t drop);  // ship bytes: 0-14: individual drop, 15: broadcast
};

extern UCBus_Head* ucBusHead;

#endif
#endif 