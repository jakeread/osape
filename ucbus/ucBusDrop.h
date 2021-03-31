/*
osap/drivers/ucBusDrop.h

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
#include "../utils/peripheralNums.h"
#include "../utils/syserror.h"
#include "ucBusDipConfig.h"

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
#define UBD_NUM_B_BUFFERS 4

#define UBD_CLOCKRESET 15

// setup 
void ucBusDrop_setup(void);

// has clear space for ucbus head to tx into?
boolean ucBusDrop_isRTR(void);

// isrs 
void ucBusDrop_rxISR(void);
void ucBusDrop_dreISR(void);
void ucBusDrop_txcISR(void);

// handlers (define in main.cpp, these are application interfaces)
void ucBusDrop_onRxISR(void);
void ucBusDrop_onPacketARx(uint8_t* inBufferA, volatile uint16_t len);

// the api, eh 
void ucBusDrop_setup(boolean useDipPick, uint8_t ID);
boolean ucBusDrop_ctrA(void);  // return true if RX complete / buffer ready 
boolean ucBusDrop_ctrB(void);
size_t ucBusDrop_readA(uint8_t *dest);  // ship les bytos
size_t ucBusDrop_readB(uint8_t *dest);
// drop cannot tx to channel A
boolean ucBusDrop_ctsB(void); // true if tx buffer empty, 
void ucBusDrop_transmitB(uint8_t *data, uint16_t len);

#endif
#endif 