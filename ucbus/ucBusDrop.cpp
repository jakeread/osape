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

// input buffer
volatile boolean lastWordAHadToken = false;
uint8_t inBufferA[UBD_BUFSIZE];
volatile uint16_t inBufferAWp = 0;
volatile uint16_t inBufferALen = 0; // writes at terminal zero, 

volatile boolean lastWordBHadToken = false;
uint8_t inBufferB[UBD_NUM_B_BUFFERS][UBD_BUFSIZE];
volatile uint16_t inBufferBLen[UBD_NUM_B_BUFFERS];
volatile unsigned long inBArrival[UBD_NUM_B_BUFFERS];
volatile uint8_t inBufferBHead = 0;
volatile uint8_t inBufferBTail = 0;
volatile uint16_t inBufferBWp = 0;

// output, 
volatile uint32_t outWord;
uint8_t outBuffer[UBD_BUFSIZE];
volatile uint16_t outBufferRp = 0;
volatile uint16_t outBufferLen = 0;

#define TOKEN_COUNT(data) (uint8_t)(data >> 30)
#define CH_COUNT(data) (data & 0b00100000000000000000000000000000) // lol 

// mask data w/ unf-bits, check if markers in place: 
#define FRAME_VALID(data) ((data & 0b11000000110000001100000011000000) == (0b00000000010000001000000011000000))
// read frame, 
#define RF_HEADER(data) (((uint8_t)(data >> 22) & 0b11111100) | ((uint8_t)(data >> 18) & 0b00000011))
#define RF_WORD0(data) (((uint8_t)(data >> 12) & 0b11110000) | ((uint8_t)(data >> 10) & 0b00001111))
#define RF_WORD1(data) (((uint8_t)(data >> 2) & 0b11000000) | ((uint8_t)(data) & 0b00111111))

// read header, 
#define HEADER_NUMRX(header) ((header >> 6) & 0b00000011)
#define HEADER_CHRX(header) (header & 0b00100000)

// available time count, 
volatile uint16_t timeTick = 0;
// timing, 
volatile uint64_t timeBlink = 0;
uint16_t blinkTime = 10000;

//void onPacketBRx(void);
// our physical bus address, 
volatile uint8_t id = 0;
volatile uint8_t rcrxb = 0; // reciprocal rx buffer 0: head has no room to rx, donot send, 1: has room
volatile unsigned long lastrc = 0;

void ucBusDrop_setup(boolean useDipPick, uint8_t ID) {
  dip_setup();
  if(useDipPick){
    // set our id, 
    id = dip_readLowerFive(); // should read lower 4, now that cha / chb 
    if(id > 31){ id = 31; };  // max 14 drops, logical addresses 0 - 14
  } else {
    id = ID;
  }
  if(id > 14){
    id = 14;
  }
  // inbuffer ringbuffer states;
  for(uint8_t i = 0; i < UBD_NUM_B_BUFFERS; i ++){
    inBufferBLen[i] = 0;
    inBArrival[i] = 0;
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
    //DEBUGPIN_ON;
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
  uint8_t numToken = HEADER_NUMRX(inHeader);
  boolean ch = HEADER_CHRX(inHeader);

  if(!ch){ // --------------------------------------------- CHA RX 
    // channel a rx, 
    if(numToken > 0 && inBufferALen != 0){
      // packet edge, reset
      DEBUG1PIN_ON;
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
        // noop
        break;
      case 1:
        lastWordAHadToken = true;
        // the next is meant to switch through: 
      case 0:
        if(lastWordAHadToken){
          lastWordAHadToken = false;
          inBufferALen = inBufferAWp;
          ucBusDrop_onPacketARx(inBufferA, inBufferALen);
          DEBUG1PIN_OFF;
        } else {
          // noop, out of frame... 
        }
    } // end switch 
  } else { // --------------------------------------------- CHB RX 
    // channel b rx, 
  }

  // ... 
  // ...
  // to transmit out, 
  //outWord = 0;
  //UBD_DRIVER_ENABLE;
  //UBD_SER_USART.DATA.reg = outWord;
  // now that we have next byte on the line, can execute this generalized application code:
  ucBusDrop_onRxISR();

  // // for now, just running the clk on every word 
  // if((data >> 7) == 0){ // timer bit, on every 0th bit, and beginning of word 
  //   inWord[0] = data;
  //   timeTick ++;
  //   timeBlink ++;
  //   if(timeBlink >= blinkTime){
  //     CLKLIGHT_TOGGLE;
  //     timeBlink = 0;
  //   }
  //   ucBusDrop_onRxISR(); // on start of each word 
  // } else { // end of word on every 0th bit 
  //   inWord[1] = data;
  //   // now decouple, 
  //   inHeader = ((inWord[0] >> 1) & 0b00111000) | ((inWord[1] >> 4) & 0b00000111);
  //   inByte = ((inWord[0] << 4) & 0b11110000) | (inWord[1] & 0b00001111);
  //   // now check incoming data, 
  //   // could speed this up to not re-evaluate (inHeader & 0b00110000) every time 
  //   if((inHeader & 0b00110000) == 0b00100000){ // --------------------------------------- has-token, CHA
  //     lastWordAHadToken = true;
  //     if(inBufferALen != 0){ // in this case, we didn't read-out of the buffer in time, 
  //       inBufferALen = 0;    // so we reset it, missing the last packet !
  //       inBufferAWp = 0;
  //     }
  //     inBufferA[inBufferAWp] = inByte;
  //     inBufferAWp ++;
  //   } else if ((inHeader & 0b00110000) == 0b00000000) { // ------------------------------ no-token, CHA
  //     if(lastWordAHadToken){
  //       inBufferALen = inBufferAWp;
  //       ucBusDrop_onPacketARx(inBufferA, inBufferALen);
  //     }
  //     lastWordAHadToken = false;
  //   } else if ((inHeader & 0b00110000) == 0b00110000) { // ------------------------------ has-token, CHB 
  //     //DEBUG1PIN_ON;
  //     lastWordBHadToken = true;
  //     if(inBufferBLen[inBufferBHead] != 0){
  //       // missed one, bad 
  //       inBufferBLen[inBufferBHead] = 0;
  //     }
  //     inBufferB[inBufferBHead][inBufferBWp] = inByte;
  //     inBufferBWp ++;
  //   } else if ((inHeader & 0b00110000) == 0b00010000) { // ------------------------------ no-token, CHB
  //     if(lastWordBHadToken){ // falling edge, packet delineation 
  //       inBufferBLen[inBufferBHead] = inBufferBWp;
  //       if(inBufferB[inBufferBHead][0] == id){ //check if pck is for us and update reciprocal buffer len 
  //         // otherwise, if head is flushing next b-ch packet out following, we start overwriting 
  //         // and hit the 'missed case' above... 
  //         rcrxb = inBufferB[inBufferBHead][1];
  //         lastrc = millis(); 
  //         inBArrival[inBufferBHead] = lastrc;
  //         // now write into next head 
  //         inBufferBHead ++;
  //         if(inBufferBHead >= UBD_NUM_B_BUFFERS){
  //           inBufferBHead = 0;
  //         }
  //         // and reset write pointer, 
  //         inBufferBWp = 0;
  //       } else {  // packet is not ours, ignore, ready for next read, don't inc head 
  //         inBufferBWp = 0;
  //         inBufferBLen[inBufferBHead] = 0;
  //       }
  //     }
  //     lastWordBHadToken = false;
  //   }
  //   // now check if outgoing tick is ours, 
  //   // possible bugfarm: if we (potentially) fire the onPacketARx() handler up there, 
  //   // and then return here to also do all of this work, we might have a l e n g t h y interrupt 
  //   if((inHeader & dropIdMask) == id){
  //     // this drop is currently 'tapped' - if it's token less, the data byte was rcrxb for us 
  //     if(!(inHeader & 0b00100000)){
  //       rcrxb = inByte;
  //       lastrc = millis();
  //     }
  //     // our transmit 
  //     if(outBufferLen > 0){
  //       // ongoing / starting transmit of bytes from our outbuffer onto the line, 
  //       outByte = outBuffer[outBufferRp];
  //       outHeader = headerMask & (tokenWord | (id & 0b00011111));
  //     } else {
  //       // whenever free space on the line, transmit whether out recieve buffer has space available 
  //       if(ucBusDrop_isRTR()){
  //         outByte = 1;  // have clear space available, communicate that to partner 
  //       } else {
  //         outByte = 0;  // currently receiving or have packet awaiting handling 
  //       }
  //       outHeader = headerMask & (noTokenWord | (id & 0b00011111));
  //     }
  //     outWord[0] = 0b00000000 | ((outHeader << 1) & 0b01110000) | (outByte >> 4);
  //     outWord[1] = 0b10000000 | ((outHeader << 4) & 0b01110000) | (outByte & 0b00001111);
  //     // now transmit, 
  //     UBD_DRIVER_ENABLE;
  //     UBD_SER_USART.DATA.reg = outWord[0];
  //     UBD_SER_USART.INTENSET.bit.DRE = 1;
  //     // now do this, 
  //     if(outBufferLen > 0){
  //       outBufferRp ++;
  //       if(outBufferRp >= outBufferLen){
  //         outBufferRp = 0;
  //         outBufferLen = 0;
  //       }
  //     }
  //   } else if ((inHeader & dropIdMask) == UBD_CLOCKRESET){
  //     // reset 
  //     timeTick = 0;
  //   }
  // } // end 1th bit case, 
  //DEBUG2PIN_OFF;
} // end rx-isr 

void SERCOM1_1_Handler(void){
  ucBusDrop_txcISR();
}

void ucBusDrop_txcISR(void){
  UBD_SER_USART.INTFLAG.bit.TXC = 1; // clear the flag by writing 1 
  //UBD_SER_USART.INTENCLR.reg = SERCOM_USART_INTENCLR_TXC; // turn off the interrupt 
  UBD_DRIVER_DISABLE; // turn off the driver to not-compete with other drops, 
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

boolean ucBusDrop_ctrA(void){
  if(inBufferALen > 0){
    return true;
  } else {
    return false;
  }
}

boolean ucBusDrop_ctrB(void){
  if(inBufferBHead == inBufferBTail){
    return false;
  } else {
    return true;
  }
}

size_t ucBusDrop_readA(uint8_t *dest){
	if(!ucBusDrop_ctrA()) return 0;
  //NVIC_DisableIRQ(SERCOM1_2_IRQn);
  __disable_irq();
  // copy out, irq disabled, TODO safety on missing an interrupt in this case?? clock jitter? 
  memcpy(dest, inBufferA, inBufferALen);
  size_t len = inBufferALen;
  inBufferALen = 0;
  inBufferAWp = 0;
  //NVIC_EnableIRQ(SERCOM1_2_IRQn);
  __enable_irq();
  return len;
}

size_t ucBusDrop_readB(uint8_t *dest){
  if(!ucBusDrop_ctrB()) return 0;
  //NVIC_DisableIRQ(SERCOM1_2_IRQn);
  __disable_irq();
  // read from the tail, 
  // bytes 0 and 1 are the ID and rcrxb, respectively, so app. is concerned with the rest 
  size_t len = inBufferBLen[inBufferBTail] - 2;
  memcpy(dest, &(inBufferB[inBufferBTail][2]), len);
  inBufferBLen[inBufferBTail] = 0;
  inBufferBWp = 0;
  inBufferBTail ++;
  if(inBufferBTail >= UBD_NUM_B_BUFFERS){
    inBufferBTail = 0;
  }
  //NVIC_EnableIRQ(SERCOM1_2_IRQn);
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
  // 1st byte: num of spaces we have to rx messages at this drop, i.e. buffer space 
  if(ucBusDrop_isRTR()){ // nothing currently reading in, nothing awaiting handle 
    outBuffer[0] = 1;
  } else { // are either currently recieving, or have recieved an unhandled packet 
    outBuffer[1] = 0;
  }
  // also decriment our accounting of their rcrxb
  rcrxb -= 1;
  memcpy(&outBuffer[1], data, len);
  // needs to be interrupt safe: transmit could start between these lines
  __disable_irq();
  outBufferLen = len + 1; // + 1 for the buffer space 
  outBufferRp = 0;
  __enable_irq();
}

#endif 