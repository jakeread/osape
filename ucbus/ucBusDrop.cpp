/*
osap/drivers/ucBusDrop.cpp

beginnings of a uart-based clock / bus combo protocol

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#include "ucBusDrop.h"

#ifdef UCBUS_IS_DROP

// input buffer A (single)
uint8_t inBufferA[UBD_BUFSIZE];
volatile uint16_t inBufferAWp = 0;
volatile uint16_t inBufferALen = 0; // writes at terminal zero, 
volatile boolean lastWordAHadToken = false;

// input buffer B (ringbuffer)
uint8_t inBufferB[UBD_NUM_B_BUFFERS][UBD_BUFSIZE];
volatile uint16_t inBufferBLen[UBD_NUM_B_BUFFERS];
volatile boolean lastWordBHadToken = false;
volatile uint8_t inBufferBHead = 0;
volatile uint8_t inBufferBTail = 0;
volatile uint16_t inBufferBWp = 0;

// output buffer 
uint8_t outBuffer[UBD_BUFSIZE];
volatile uint16_t outBufferRp = 0;
volatile uint16_t outBufferLen = 0;

// outgoing word 
volatile uint32_t outFrame;

// reciprocal buffer space, for flowcontrol 
volatile uint8_t rcrxb = 0;

// our physical bus address, 
volatile uint8_t id = 0;

// available time count, 
volatile uint16_t timeTick = 0;
volatile uint64_t timeBlink = 0;
uint16_t blinkTime = 10000;

// mask data w/ unf-bits, check if markers in place: 
#define FRAME_VALID(data) ((data & 0b11000000110000001100000011000000) == (0b00000000010000001000000011000000))
// read frame, 
#define RF_HEADER(data) (((uint8_t)(data >> 22) & 0b11111100) | ((uint8_t)(data >> 20) & 0b00000011))
#define RF_WORD0(data) (((uint8_t)(data >> 12) & 0b11110000) | ((uint8_t)(data >> 10) & 0b00001111))
#define RF_WORD1(data) (((uint8_t)(data >> 2) & 0b11000000) | ((uint8_t)(data) & 0b00111111))

// transmit frame ... supposing its always 0s that are shifted in, 
#define WF_TOKEN(token, id, d0, d1, d2) ( 0 | (((uint32_t)(token & 0b00000011)) << 30) | (((uint32_t)(id & 0b00011111)) << 24) | (((uint32_t)d0) << 16) | (((uint32_t)d1) << 8) | ((uint32_t)(d2)) )
#define WF_NOTOKEN(id, space) ( 0 | (((uint32_t)(id & 0b00011111)) << 24) | ((uint32_t)space) )

// read header, 
#define HEADER_RX_TOKEN(header) ((header & 0b11000000) >> 6)
#define HEADER_RX_DROPTAP(header) (header & 0b00011111)
#define HEADER_RX_CH(header) (header & 0b00100000)

void ucBusDrop_setup(boolean useDipPick, uint8_t ID) {
  dip_setup();
  if(useDipPick){
    // set our id, 
    id = dip_readLowerFive(); // should read lower 4, now that cha / chb 
  } else {
    id = ID;
  }
  if(id > 31){ id = 31; }   // max 31 drops, logical addresses 1 - 31
  if(id == 0){ id = 1; }    // 0 'tap' is the clk reset, bump up... maybe cause confusion: instead could flash err light 

  // inbuffer ringbuffer states;
  for(uint8_t i = 0; i < UBD_NUM_B_BUFFERS; i ++){
    inBufferBLen[i] = 0;
  }
  // set driver output LO to start: tri-state 
  UBD_DE_PORT.DIRSET.reg = UBD_DE_BM;
  UBD_DRIVER_DISABLE;
  // set receiver output on, forever: LO to set on 
  UBD_RE_PORT.DIRSET.reg = UBD_RE_BM;
  UBD_RE_PORT.OUTCLR.reg = UBD_RE_BM;
  // termination resistor should be set only on one drop, 
  // or none and physically with a 'tail' cable, or something? 
  UBD_TE_PORT.DIRSET.reg = UBD_TE_BM;
  if(dip_readPin1()){
    UBD_TE_PORT.OUTCLR.reg = UBD_TE_BM;
  } else {
    UBD_TE_PORT.OUTSET.reg = UBD_TE_BM;
  }
  // rx pin setup
  UBD_COMPORT.DIRCLR.reg = UBD_RXBM;
  UBD_COMPORT.PINCFG[UBD_RXPIN].bit.PMUXEN = 1;
  if(UBD_RXPIN % 2){
    UBD_COMPORT.PMUX[UBD_RXPIN >> 1].reg |= PORT_PMUX_PMUXO(UBD_RXPERIPHERAL);
  } else {
    UBD_COMPORT.PMUX[UBD_RXPIN >> 1].reg |= PORT_PMUX_PMUXE(UBD_RXPERIPHERAL);
  }
  // tx
  UBD_COMPORT.DIRCLR.reg = UBD_TXBM;
  UBD_COMPORT.PINCFG[UBD_TXPIN].bit.PMUXEN = 1;
  if(UBD_TXPIN % 2){
    UBD_COMPORT.PMUX[UBD_TXPIN >> 1].reg |= PORT_PMUX_PMUXO(UBD_TXPERIPHERAL);
  } else {
    UBD_COMPORT.PMUX[UBD_TXPIN >> 1].reg |= PORT_PMUX_PMUXE(UBD_TXPERIPHERAL);
  }
  // ok, clocks, first line au manuel
  	// unmask clocks 
	MCLK->APBAMASK.bit.SERCOM1_ = 1;
  GCLK->GENCTRL[UBD_GCLKNUM_PICK].reg = GCLK_GENCTRL_SRC(GCLK_GENCTRL_SRC_DFLL) | GCLK_GENCTRL_GENEN;
  while(GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL(UBD_GCLKNUM_PICK));
	GCLK->PCHCTRL[UBD_SERCOM_CLK].reg = GCLK_PCHCTRL_CHEN | GCLK_PCHCTRL_GEN(UBD_GCLKNUM_PICK);
  // then, sercom
  while(UBD_SER_USART.SYNCBUSY.bit.ENABLE);
  UBD_SER_USART.CTRLA.bit.ENABLE = 0;
  while(UBD_SER_USART.SYNCBUSY.bit.SWRST);
  UBD_SER_USART.CTRLA.bit.SWRST = 1;
  while(UBD_SER_USART.SYNCBUSY.bit.SWRST);
  while(UBD_SER_USART.SYNCBUSY.bit.SWRST || UBD_SER_USART.SYNCBUSY.bit.ENABLE);
  // ctrla 
  UBD_SER_USART.CTRLA.reg = SERCOM_USART_CTRLA_MODE(1) | SERCOM_USART_CTRLA_DORD;
  UBD_SER_USART.CTRLA.reg |= SERCOM_USART_CTRLA_RXPO(UBD_RXPO) | SERCOM_USART_CTRLA_TXPO(0);
  UBD_SER_USART.CTRLA.reg |= SERCOM_USART_CTRLA_FORM(1); // enable even parity 
  // ctrlb 
  while(UBD_SER_USART.SYNCBUSY.bit.CTRLB);
  UBD_SER_USART.CTRLB.reg = SERCOM_USART_CTRLB_RXEN | SERCOM_USART_CTRLB_TXEN | SERCOM_USART_CTRLB_CHSIZE(0);
  // ctrlc for 32bit 
  UBD_SER_USART.CTRLC.reg |= SERCOM_USART_CTRLC_DATA32B(3);
	// enable interrupts 
	NVIC_EnableIRQ(SERCOM1_2_IRQn); // rx, 
  NVIC_EnableIRQ(SERCOM1_1_IRQn); // tx complete 
	// set baud 
  UBD_SER_USART.BAUD.reg = UBD_BAUD_VAL;
  // and finally, a kickoff
  while(UBD_SER_USART.SYNCBUSY.bit.ENABLE);
  UBD_SER_USART.CTRLA.bit.ENABLE = 1;
  // enable rx interrupt 
  UBD_SER_USART.INTENSET.bit.RXC = 1;
  // to enable tx complete, 
  UBD_SER_USART.INTENSET.reg = SERCOM_USART_INTENSET_TXC; // now watch transmit complete
}

void SERCOM1_2_Handler(void){
  //ERRLIGHT_TOGGLE;
	ucBusDrop_rxISR();
}

void ucBusDrop_rxISR(void){
  // check parity bit,
  uint16_t perr = UBD_SER_USART.STATUS.bit.PERR;
  if(perr){
    uint8_t clear = UBD_SER_USART.DATA.reg;
    UBD_SER_USART.STATUS.bit.PERR = 1; // clear parity error 
    return;
  } 
	// cleared by reading out, but we are blind feed forward atm 
  // get the data 
  uint32_t data = UBD_SER_USART.DATA.reg;
  // every tick, 
  timeTick ++;
  timeBlink ++;
  if(timeBlink >= blinkTime){
    CLKLIGHT_TOGGLE; 
    ERRLIGHT_OFF;
    timeBlink = 0;
  }

  // we can have this case where the frame is not properly aligned, check / correct by resetting 
  // and hopefully eventually catch the edge properly... 
  // this should only happen once, on startup... 
  if(!FRAME_VALID(data)){
    // reset 
    ERRLIGHT_ON;
    while(UBD_SER_USART.SYNCBUSY.bit.ENABLE);
    UBD_SER_USART.CTRLA.bit.ENABLE = 0;
    while(UBD_SER_USART.SYNCBUSY.bit.ENABLE);
    UBD_SER_USART.CTRLA.bit.ENABLE = 1;
    return;
  } 

  // reclaim header & data bytes, 
  uint8_t inHeader = RF_HEADER(data);
  uint8_t inWord[2] = {RF_WORD0(data), RF_WORD1(data)};

  // count number of rx'd in & ch, 
  uint8_t numToken = HEADER_RX_TOKEN(inHeader);
  boolean ch = HEADER_RX_CH(inHeader);
  uint8_t dropTap = HEADER_RX_DROPTAP(inHeader);

  // ------------------------------------------------------ RX TERMS
  if(!ch){ // --------------------------------------------- CHA RX 
    // channel a rx, 
    if(numToken > 0 && inBufferALen != 0){
      // packet edge, reset
      //DEBUG1PIN_ON;
      inBufferALen = 0;
      inBufferAWp = 0;
    }
    // write in 
    for(uint8_t i = 0; i < numToken; i ++){
      inBufferA[inBufferAWp ++] = inWord[i];
    }
    // switch on # tokens
    switch(numToken){
      case 2:
        lastWordAHadToken = true;
        break;
      case 1:
        lastWordAHadToken = true;
        // the next is meant to switch through: 
      case 0:
        if(lastWordAHadToken){
          lastWordAHadToken = false;
          // set packet fullness, execute 
          inBufferALen = inBufferAWp;
          ucBusDrop_onPacketARx(inBufferA, inBufferALen);
          //DEBUG1PIN_OFF;
        } else {
          // noop, out of frame
        }
        break;
    } // end cha token count switch 
  } else { // --------------------------------------------- CHB RX 
    // channel b rx, 
    if(numToken > 0 && inBufferBLen[inBufferBHead] != 0){
      inBufferBLen[inBufferBHead] = 0;
      inBufferBWp = 0;
    }
    // write in 
    for(uint8_t i = 0; i < numToken; i ++){
      inBufferB[inBufferBHead][inBufferBWp ++] = inWord[i];
    }
    // switch on # tokens, 
    switch(numToken){
      case 2:
        lastWordBHadToken = true;
        break;
      case 1:
        lastWordBHadToken = true; // meant to switch thru, 
      case 0:
        if(lastWordBHadToken){
          lastWordBHadToken = false;
          // if was for us, 
          if(inBufferB[inBufferBHead][0] == id){
            // set packet fullness, increment ringbuffer, 2nd byte is rcrxb for us 
            rcrxb = inBufferB[inBufferBHead][1];
            inBufferBLen[inBufferBHead] = inBufferBWp;
            inBufferBWp = 0;
            inBufferBHead ++;
            if(inBufferBHead >= UBD_NUM_B_BUFFERS){
              inBufferBHead = 0;
            }
          } else {
            // not our packet, set to write next, 
            inBufferBWp = 0;
          }
        } else {
          // noop, out of frame, 
        }
        break;
    } // end chb token count switch 
  } // ---------------------------------------------------- END RX TERMS 

  // ------------------------------------------------------ CHECK FOR TX 
  if(dropTap == id){
    // our turn in time, forumlate byte & git it oot 
    DEBUG1PIN_ON;
    // 1st op: if numToken < 2, word[1] is rcrxb for us, 
    if(numToken < 2) rcrxb = inWord[1];
    // now, if we have outgoing to tx: 
    if(outBufferLen > 0){ // transmit out if it's present, 
      // num to transmit, 
      uint8_t numTx = outBufferLen - outBufferRp;
      if(numTx > 3) numTx = 3;
      if(numTx < 3){ // when on tail of pck edge, fill last byte w/ rcrxb for us for head 
        outFrame = WF_TOKEN(numTx, id, outBuffer[outBufferRp], outBuffer[outBufferRp + 1], (ucBusDrop_isRTR() ? 1 : 0));
        outBufferLen = 0;
        outBufferRp = 0;
      } else {
        // full stack, tx on all words 
        outFrame = WF_TOKEN(numTx, id, outBuffer[outBufferRp], outBuffer[outBufferRp + 1], outBuffer[outBufferRp + 2]);
        outBufferRp += numTx;
      }
    } else {  // otherwise, transmit our chb recieve side flowcontrol info w/ no token 
      outFrame = WF_NOTOKEN(id, (ucBusDrop_isRTR() ? 1 : 0));
    }
    UBD_DRIVER_ENABLE;
    UBD_SER_USART.DATA.reg = outFrame;
  }

  // ... 
  // ...
  // now that we have next byte on the line, can execute this generalized application code:
  ucBusDrop_onRxISR();
} // end rx-isr 

void SERCOM1_1_Handler(void){
  ucBusDrop_txcISR();
}

void ucBusDrop_txcISR(void){
  UBD_SER_USART.INTFLAG.bit.TXC = 1; // clear the flag by writing 1 
  //UBD_SER_USART.INTENCLR.reg = SERCOM_USART_INTENCLR_TXC; // turn off the interrupt 
  UBD_DRIVER_DISABLE; // turn off the driver to not-compete with other drops, 
  DEBUG1PIN_OFF;
}

// check buffer state: is it OK for us to rx new pcks from head? 
boolean ucBusDrop_isRTR(void){
  // if *current && next head* is empty (i.e. have at least two spaces)
  if(inBufferBLen[inBufferBHead] == 0){
    uint8_t next = inBufferBHead + 1;
    if(next >= UBD_NUM_B_BUFFERS){
      next = 0;
    }
    if(inBufferBLen[next] == 0){
      return true;
    } 
  } 
  return false;
}

// -------------------------------------------------------- ASYNC API

boolean ucBusDrop_ctrB(void){
  if(inBufferBHead == inBufferBTail){ // head == tail when the thing is empty 
    return false;
  } else {
    return true;
  }
}

size_t ucBusDrop_readB(uint8_t *dest){
  if(!ucBusDrop_ctrB()) return 0;
  // read from the tail, 
  // bytes 0 and 1 are the ID and rcrxb, respectively, so app. is concerned with the rest 
  size_t len = inBufferBLen[inBufferBTail] - 2;
  memcpy(dest, &(inBufferB[inBufferBTail][2]), len);
  inBufferBLen[inBufferBTail] = 0;
  __disable_irq(); // shouldn't need to guard here, right? not sure 
  inBufferBTail ++;
  if(inBufferBTail >= UBD_NUM_B_BUFFERS){
    inBufferBTail = 0;
  }
  __enable_irq();
  return len;
}

boolean ucBusDrop_ctsB(void){
  if(outBufferLen == 0 && rcrxb > 0){
    return true;
  } else {
    return false;
  }
}

void ucBusDrop_transmitB(uint8_t *data, uint16_t len){
  if(!ucBusDrop_ctsB()){ return; }
  // decriment our accounting of their rcrxb, 
  rcrxb -= 1;
  memcpy(outBuffer, data, len);
  // needs to be interrupt safe: transmit could start between these lines
  __disable_irq();
  outBufferLen = len; // + 1 for the buffer space 
  outBufferRp = 0;
  __enable_irq();
}

#endif 