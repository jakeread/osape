/*
osap/drivers/ucBusHead.h

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
#include "../utils/peripheralNums.h"
#include "../utils/syserror.h"
#include "../utils/d51ClockBoss.h"

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

#ifdef IS_OG_MODULE 
#define UBH_DE_PIN 16 // driver output enable: set HI to enable, LO to tri-state the driver 
#define UBH_DE_PORT PORT->Group[1] 
#define UBH_RE_PIN 19 // receiver output enable, set LO to enable the RO, set HI to tri-state RO 
#define UBH_RE_PORT PORT->Group[0]
#else 
#define UBH_DE_PIN 19 // driver output enable: set HI to enable, LO to tri-state the driver 
#define UBH_DE_PORT PORT->Group[0] 
#define UBH_RE_PIN 9 // receiver output enable, set LO to enable the RO, set HI to tri-state RO 
#define UBH_RE_PORT PORT->Group[1]
#endif 

#define UBH_TE_PIN 17  // termination enable, drive LO to enable to internal termination resistor, HI to disable
#define UBH_TE_PORT PORT->Group[0]
#define UBH_TE_BM (uint32_t)(1 << UBH_TE_PIN)
#define UBH_RE_BM (uint32_t)(1 << UBH_RE_PIN)
#define UBH_DE_BM (uint32_t)(1 << UBH_DE_PIN)

#define UBH_BUFSIZE 1024

// # of drops to look for 
// count 31: 2^5 bits, with one case reserved for the clock reset (id 0)
#define UBH_DROP_OPS 31

// setup, 
void ucBusHead_setup(void);

// need to call the main timer isr from some tick, normally 100kHz 
void ucBusHead_timerISR(void);
void ucBusHead_rxISR(void);
void ucBusHead_txISR(void);

// ub interface, 
boolean ucBusHead_ctr(uint8_t drop); // is there ahn packet to read at this drop 
size_t ucBusHead_read(uint8_t drop, uint8_t *dest);  // get 'them bytes fam 
//size_t ucBusHead_readPtr(uint8_t* drop, uint8_t** dest, unsigned long *pat); // vport interface, get next to handle... 
//void ucBusHead_clearPtr(uint8_t drop);
boolean ucBusHead_ctsA(void);  // return true if TX complete / buffer ready
boolean ucBusHead_ctsB(uint8_t drop);
void ucBusHead_transmitA(uint8_t *data, uint16_t len);  // ship bytes: broadcast to all 
void ucBusHead_transmitB(uint8_t *data, uint16_t len, uint8_t drop);  // ship bytes: 0-14: individual drop, 15: broadcast

#endif
#endif 