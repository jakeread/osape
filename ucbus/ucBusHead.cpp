/*
osap/drivers/ucBusHead.cpp

beginnings of a uart-based clock / bus combo protocol

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#include "ucBusHead.h"

#ifdef UCBUS_IS_HEAD

// input buffers / space 
volatile uint8_t inWord[2];
volatile uint8_t inHeader;
volatile uint8_t inByte;
uint8_t inBuffer[UBH_DROP_OPS][UBH_BUFSIZE];  // per-drop incoming bytes 
volatile uint16_t inBufferWp[UBH_DROP_OPS];   // per-drop incoming write pointer
volatile uint16_t inBufferLen[UBH_DROP_OPS];  // per-drop incoming bytes, len of, set when EOP detected
volatile unsigned long inArrivalTime[UBH_DROP_OPS];
volatile boolean inLastHadToken[UBH_DROP_OPS];

// transmit buffers for A / B Channels 
uint8_t outBufferA[UBH_BUFSIZE];
volatile uint16_t outBufferARp = 0;
volatile uint16_t outBufferALen = 0;
uint8_t outBufferB[UBH_BUFSIZE];
volatile uint16_t outBufferBRp = 0;
volatile uint16_t outBufferBLen = 0;

// doublet outgoing word / state 
volatile uint32_t outWord = 0;
volatile uint8_t currentDropTap = 0; // drop we are currently 'txing' to / drop that will reply on this cycle

// word masks 
#define HEADER_DROP_TAP(drop) (drop << 24)
#define HEADER_CHA_TOKEN(count) (count >= 3) ? (3 << 30) : (count << 30)
#define FRAME_BYTE_FILL(pos, byte) ((uint32_t)byte << (pos * 8))

const uint8_t headerMask =    0b00111111;
const uint8_t dropIdMask =    0b00001111; 
                              // 0b00|token|channel|4bit id
const uint8_t tokenWordA =    0b00100000; // CHA, data byte present 
const uint8_t noTokenWordA =  0b00000000; // CHA, data byte not present 
const uint8_t tokenWordB =    0b00110000; // CHB, data byte present 
const uint8_t noTokenWordB =  0b00010000; // CHB, data byte not present 
volatile uint8_t lastSpareEOP = 0;        // last channel we transmitted spare end-of-packet on

// reciprocal recieve buffer spaces 
volatile uint8_t rcrxb[UBH_DROP_OPS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long lastrc[UBH_DROP_OPS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// uart init (file scoped)
void setupUART(void){
  // driver output is always on at head, set HI to enable
  UBH_DE_PORT.DIRSET.reg = UBH_DE_BM;
  UBH_DE_PORT.OUTSET.reg = UBH_DE_BM;
  // receive output is always on at head, set LO to enable
  UBH_RE_PORT.DIRSET.reg = UBH_RE_BM;
  UBH_RE_PORT.OUTCLR.reg = UBH_RE_BM;
  // termination resistor for receipt on bus head is on, set LO to enable 
  UBH_TE_PORT.DIRSET.reg = UBH_TE_BM;
  UBH_TE_PORT.OUTCLR.reg = UBH_TE_BM;
  // rx pin setup
  UBH_COMPORT.DIRCLR.reg = UBH_RXBM;
  UBH_COMPORT.PINCFG[UBH_RXPIN].bit.PMUXEN = 1;
  if(UBH_RXPIN % 2){
    UBH_COMPORT.PMUX[UBH_RXPIN >> 1].reg |= PORT_PMUX_PMUXO(UBH_RXPERIPHERAL);
  } else {
    UBH_COMPORT.PMUX[UBH_RXPIN >> 1].reg |= PORT_PMUX_PMUXE(UBH_RXPERIPHERAL);
  }
  // tx
  UBH_COMPORT.DIRCLR.reg = UBH_TXBM;
  UBH_COMPORT.PINCFG[UBH_TXPIN].bit.PMUXEN = 1;
  if(UBH_TXPIN % 2){
    UBH_COMPORT.PMUX[UBH_TXPIN >> 1].reg |= PORT_PMUX_PMUXO(UBH_TXPERIPHERAL);
  } else {
    UBH_COMPORT.PMUX[UBH_TXPIN >> 1].reg |= PORT_PMUX_PMUXE(UBH_TXPERIPHERAL);
  }
  // ok, clocks, first line au manuel
  // unmask clocks 
	MCLK->APBAMASK.bit.SERCOM1_ = 1;
  GCLK->GENCTRL[UBH_GCLKNUM_PICK].reg = GCLK_GENCTRL_SRC(GCLK_GENCTRL_SRC_DFLL) | GCLK_GENCTRL_GENEN;
  while(GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL(UBH_GCLKNUM_PICK));
	GCLK->PCHCTRL[UBH_SERCOM_CLK].reg = GCLK_PCHCTRL_CHEN | GCLK_PCHCTRL_GEN(UBH_GCLKNUM_PICK);
  // then, sercom: disable and then perform software reset
  while(UBH_SER_USART.SYNCBUSY.bit.ENABLE);
  UBH_SER_USART.CTRLA.bit.ENABLE = 0;
  while(UBH_SER_USART.SYNCBUSY.bit.SWRST);
  UBH_SER_USART.CTRLA.bit.SWRST = 1;
  while(UBH_SER_USART.SYNCBUSY.bit.SWRST);
  while(UBH_SER_USART.SYNCBUSY.bit.SWRST || UBH_SER_USART.SYNCBUSY.bit.ENABLE);
  // ok, CTRLA:
  UBH_SER_USART.CTRLA.reg = SERCOM_USART_CTRLA_MODE(1) | SERCOM_USART_CTRLA_DORD; // data order (1: lsb first) and mode (?) 
  UBH_SER_USART.CTRLA.reg |= SERCOM_USART_CTRLA_RXPO(UBH_RXPO) | SERCOM_USART_CTRLA_TXPO(0); // rx and tx pinout options 
  UBH_SER_USART.CTRLA.reg |= SERCOM_USART_CTRLA_FORM(1); // turn on parity: parity is even by default (set in CTRLB), leave that 
  // CTRLB has sync bit, 
  while(UBH_SER_USART.SYNCBUSY.bit.CTRLB);
  // recieve enable, txenable, character size 8bit, 
  UBH_SER_USART.CTRLB.reg = SERCOM_USART_CTRLB_RXEN | SERCOM_USART_CTRLB_TXEN | SERCOM_USART_CTRLB_CHSIZE(0);
  // CTRLC: setup 32 bit on read and write:
  UBH_SER_USART.CTRLC.reg = SERCOM_USART_CTRLC_DATA32B(3); 
	// enable interrupts 
	//NVIC_EnableIRQ(SERCOM1_2_IRQn); // tx complete interrupts, won't be using these w/ 32bit... 
	NVIC_EnableIRQ(SERCOM1_0_IRQn);
	// set baud 
  UBH_SER_USART.BAUD.reg = UBH_BAUD_VAL;
  // and finally, a kickoff
  while(UBH_SER_USART.SYNCBUSY.bit.ENABLE);
  UBH_SER_USART.CTRLA.bit.ENABLE = 1;
  // enable the rx interrupt, 
	UBH_SER_USART.INTENSET.bit.RXC = 1;
}

// TX Handler, for second bytes initiated by timer, 
// void SERCOM1_0_Handler(void){
// 	ucBusHead_txISR();
// }

// rx handler, for incoming
void SERCOM1_2_Handler(void){
	ucBusHead_rxISR();
}

// startup, 
void ucBusHead_setup(void){
// clear buffers to begin,
  for(uint8_t d = 0; d < UBH_DROP_OPS; d ++){
    inBufferLen[d] = 0;
    inBufferWp[d] = 0;
  }
  // start the uart, 
  setupUART();
  // ! alert ! need to setup timer in main.cpp 
}

uint32_t count = 0;

void ucBusHead_timerISR(void){
  DEBUG3PIN_HI;
  // first: on each transmit, we 'tap' some drop, it's their turn to reply: 
  // also init the outgoing word with this call, 
  outWord = 0 | HEADER_DROP_TAP(currentDropTap);
  // increment that, 
  currentDropTap ++;
  if(currentDropTap > 31){
    currentDropTap = 0;
  }
  // always do cha bytes first, 
  if(outBufferALen > 0){
    DEBUG2PIN_TOGGLE;
    // tx from cha
    uint8_t numTx = outBufferALen - outBufferARp;
    if(numTx > 3) numTx = 3;
    //fill bytes accordingly, 
    for(uint8_t i = 0; i < numTx; i ++){
      outWord |= FRAME_BYTE_FILL(i, outBufferA[outBufferARp ++]);
    }
    // check / increment posn w/r/t buffer
    if(numTx < 3){
      // packet will terminate with this frame, 
      outWord |= HEADER_CHA_TOKEN(numTx);
      // terminated, so can reset these 
      outBufferALen = 0;
      outBufferARp = 0;
    } else {
      // maybe mid-packet, write and increment 
      outWord |= HEADER_CHA_TOKEN(3);
    }
  } else if (outBufferBLen > 0){
    // tx from chb
    outWord = 0;
  } else {
    // nothing to tx, 
    outWord = 0;
  }
  // finally, do the action 
  UBH_SER_USART.DATA.reg = outWord;
  DEBUG3PIN_LO;
  // // debug zero
  // if(currentDropTap == 0){
  //   //DEBUG1PIN_TOGGLE;
  // }
  // // so, would formulate the out bytes,
  // // in either case we formulate the outByte and outHeader, then bit shift identically into 
  // // the word ... 
  // __disable_irq();
  // if(outBufferALen > 0){ // always transmit channel A before B 
  //   // have bytes, write word to encapsulate outBuffer[outBufferRp]; the do outBufferRp ++ and check wrap 
  //   // mask: 6 bit, 
  //   if(outBufferARp >= outBufferALen){
  //     // this is the EOP frame, 
  //     outByte = 0;
  //     outHeader = headerMask & (noTokenWordA | (currentDropTap & dropIdMask));
  //     // now it's clear, 
  //     outBufferARp = 0;
  //     outBufferALen = 0;
  //   } else {
  //     // this is a regular frame 
  //     outByte = outBufferA[outBufferARp];
  //     outHeader = headerMask & (tokenWordA | (currentDropTap & dropIdMask));
  //     outBufferARp ++; // increment for next byte, 
  //   }
  // } else if (outBufferBLen > 0){
  //   if(outBufferBRp >= outBufferBLen){
  //     DEBUG2PIN_TOGGLE;
  //     // CHB EOP frame 
  //     outByte = 0;
  //     outHeader = headerMask & (noTokenWordB | (currentDropTap & dropIdMask));
  //     // now is clear, 
  //     outBufferBRp = 0;
  //     outBufferBLen = 0;
  //   } else {
  //     //DEBUG2PIN_ON;
  //     // ahn regular CHB frame 
  //     outByte = outBufferB[outBufferBRp];
  //     outHeader = headerMask & (tokenWordB | (currentDropTap & dropIdMask));
  //     outBufferBRp ++;
  //   }
  // } else {
  //   // no token, no EOP on either channel 
  //   // transmit our recieve buffer spaces available to this drop using the out byte 
  //   if(inBufferLen[currentDropTap] == 0 && inBufferWp[currentDropTap] == 0){
  //     outByte = 1;
  //   } else {
  //     outByte = 0;
  //   }
  //   // alternate channels, in case spurious packets not closed on one ... ensure close 
  //   if(lastSpareEOP == 0){
  //     outHeader = headerMask & (noTokenWordA | (currentDropTap & dropIdMask));
  //     lastSpareEOP = 1;
  //   } else {
  //     outHeader = headerMask & (noTokenWordB | (currentDropTap & dropIdMask));
  //     lastSpareEOP = 0;
  //   }
  // }
  // outWord[0] = 0b00000000 | ((outHeader << 1) & 0b01110000) | (outByte >> 4);
  // outWord[1] = 0b10000000 | ((outHeader << 4) & 0b01110000) | (outByte & 0b00001111);
  // __enable_irq();
  // // put the UART to work before more clocks on buffer incrementing 
  // UBH_SER_USART.DATA.reg = outWord[0];
  // // and setup the interrupt to handle the second, 
  // UBH_SER_USART.INTENSET.bit.DRE = 1;
  // // and loop through returns, 
  // currentDropTap ++;
  // if(currentDropTap >= UBH_DROP_OPS){
  //   currentDropTap = 0;
  // }
}

// void ucBusHead_txISR(void){
//   UBH_SER_USART.DATA.reg = outWord[1]; // just services the next byte in the word: timer initiates 
//   UBH_SER_USART.INTENCLR.reg = SERCOM_USART_INTENCLR_DRE; // turn this interrupt off 
// }

void ucBusHead_rxISR(void){
  // check parity bit,
  uint16_t perr = UBH_SER_USART.STATUS.bit.PERR;
  if(perr){
    //ERRLIGHT_ON;
    uint32_t clear = UBH_SER_USART.DATA.reg;
    UBH_SER_USART.STATUS.bit.PERR = 1; // clear parity flag 
    return;
  }
	// cleared by reading out
  // get the byte, 
  uint32_t data = UBH_SER_USART.DATA.reg;

  // if((data >> 7) == 0){
  //   inWord[0] = data;
  // } else {
  //   inWord[1] = data;
  //   // now decouple, 
  //   inHeader = ((inWord[0] >> 1) & 0b00111000) | ((inWord[1] >> 4) & 0b00000111);
  //   inByte = ((inWord[0] << 4) & 0b11110000) | (inWord[1] & 0b00001111);
  //   // the drop reporting 
  //   uint8_t drop = inHeader & dropIdMask;
  //   // if reported drop is greater than # of max drops, something strange,
  //   if(drop > UBH_DROP_OPS) return;
  //   // otherwise, should check if has a token, right? 
  //   if(inHeader & tokenWordA){ // token, 
  //     inLastHadToken[drop] = true;
  //     if(inBufferLen[drop] != 0){ // didn't read the last in time, will drop 
  //       inBufferLen[drop] = 0;
  //       inBufferWp[drop] = 0;
  //     }
  //     inBuffer[drop][inBufferWp[drop]] = inByte; // stuff bytes 
  //     inBufferWp[drop] += 1;
  //   } else { // no token, 
  //     if(inLastHadToken[drop]){ // falling edge, packet delineation 
  //       rcrxb[drop] = inBuffer[drop][0]; // 1st byte of the packet was the rcrxb (reciprocal recieve buffer size)
  //       unsigned long arrivalTime = millis();
  //       lastrc[drop] = arrivalTime;
  //       inArrivalTime[drop] = arrivalTime;
  //       inBufferLen[drop] = inBufferWp[drop]; // this signals to outside observers that we are packet-ful
  //     }
  //     inLastHadToken[drop] = false;
  //     // on token-less words, the data byte is the rcrxb: that way this is always updating if 
  //     // out-of-packet spaces exist. otherwise it updates w/ the first byte of each packet 
  //     rcrxb[drop] = inByte;
  //     lastrc[drop] = millis();
  //   }
  // }
}

// drop transmits like so:
/*
if(outBufferLen > 0){
        outByte = outBuffer[outBufferRp];
        outHeader = headerMask & (tokenWord | (id & 0b00011111));
      } else {
        outByte = 0;
        outHeader = headerMask & (noTokenWord | (id & 0b00011111));
      }
      outWord[0] = 0b00000000 | ((outHeader << 1) & 0b01110000) | (outByte >> 4);
      outWord[1] = 0b10000000 | ((outHeader << 4) & 0b01110000) | (outByte & 0b00001111);
*/

// -------------------------------------------------------- API 

// clear to read ? 
boolean ucBusHead_ctr(uint8_t drop){
  if(drop >= UBH_DROP_OPS) return false;
  if(inBufferLen[drop] > 0){
    return true;
  } else {
    return false;
  }
}

size_t ucBusHead_read(uint8_t drop, uint8_t *dest){
  if(!ucBusHead_ctr(drop)) return 0;
  //NVIC_DisableIRQ(SERCOM1_2_IRQn);
  __disable_irq();
  // byte 1 is the drop's rcrxb transmitted with this packet
  size_t len = inBufferLen[drop] - 1;
  memcpy(dest, &(inBuffer[drop][1]), len);
  inBufferLen[drop] = 0;
  inBufferWp[drop] = 0;
  //NVIC_EnableIRQ(SERCOM1_2_IRQn);
  __enable_irq();
  return len;
}

// mod cts(channel) and transmit(data, len, channel)
// then do an example for channel-b-write currents, then do drop code, then test 

boolean ucBusHead_ctsA(void){
	if(outBufferALen == 0){ // only condition is that our transmit buffer is zero, are not currently tx'ing on this channel 
		return true;
	} else {
		return false;
	}
}

boolean ucBusHead_ctsB(uint8_t drop){
  // escape states 
  // sysError("drop: " + String(drop) + " obl: " + String(outBufferBLen) + " rc: " + String(rcrxb[0]) + " " + String(rcrxb[1]) + " " + String(rcrxb[2]) + " ");
  if(outBufferBLen == 0 && rcrxb[drop] > 0){
    return true; 
  } else {
    return false;
  }
}

void ucBusHead_transmitA(uint8_t *data, uint16_t len){
	if(!ucBusHead_ctsA()) return;
  // force reset while not operating, 
  __disable_irq();
  outBufferARp = 0;
  __enable_irq();
  // copy in, 
  memcpy(outBufferA, data, len);
  // now set length, 
  __disable_irq();
  outBufferALen = len;
  outBufferARp = 0;
  __enable_irq();
}

void ucBusHead_transmitB(uint8_t *data, uint16_t len, uint8_t drop){
  if(!ucBusHead_ctsB(drop)) return;
  #warning also curious about this potentially long-time disabled interrupt... 
  __disable_irq();
  // transmits w/ two addnl bytes: 
  // 1st byte: drop identifier 
  outBufferB[0] = drop;
  // 2nd byte: number of spaces present here (in the head) that drop can transmit into, at the moment this is 1 or 0 
  if(inBufferLen[drop] == 0 && inBufferWp[drop] == 0){
    outBufferB[1] = 1;  // no packet awaiting read (len-full) or currently writing into (wp-full)
  } else {
    outBufferB[1] = 0;  // packet either mid-recieve (wp nonzero) or is awaiting read (len-full)
  }
  memcpy(&(outBufferB[2]), data, len);
  outBufferBLen = len + 2; // + 1 for the drop + 1 for the spaces 
  outBufferBRp = 0;
  __enable_irq();
}

#endif 