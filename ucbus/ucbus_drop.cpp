/*
osap/drivers/ucbus_head.cpp

beginnings of a uart-based clock / bus combo protocol

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#include "ucbus_drop.h"

#ifdef UCBUS_IS_DROP

UCBus_Drop* UCBus_Drop::instance = 0;

UCBus_Drop* UCBus_Drop::getInstance(void){
	if(instance == 0){
		instance = new UCBus_Drop();
	}
	return instance;
}

// making this instance globally available, when built, 
// recall extern declaration in .h 
UCBus_Drop* ucBusDrop = UCBus_Drop::getInstance();

UCBus_Drop::UCBus_Drop(void) {}

void UCBus_Drop::init(boolean useDipPick, uint8_t ID) {
  dip_init();
  if(useDipPick){
    // set our id, 
    id = dip_read_lower_five();
  } else {
    id = ID;
  }
  if(id > 14){
    id = 14;
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
  if(dip_read_pin_1()){
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
  // ok, well
  UBD_SER_USART.CTRLA.reg = SERCOM_USART_CTRLA_MODE(1) | SERCOM_USART_CTRLA_DORD;
  UBD_SER_USART.CTRLA.reg |= SERCOM_USART_CTRLA_RXPO(UBD_RXPO) | SERCOM_USART_CTRLA_TXPO(0);
  UBD_SER_USART.CTRLA.reg |= SERCOM_USART_CTRLA_FORM(1); // enable even parity 
  while(UBD_SER_USART.SYNCBUSY.bit.CTRLB);
  UBD_SER_USART.CTRLB.reg = SERCOM_USART_CTRLB_RXEN | SERCOM_USART_CTRLB_TXEN | SERCOM_USART_CTRLB_CHSIZE(0);
	// enable interrupts 
	NVIC_EnableIRQ(SERCOM1_2_IRQn); // rx, 
  NVIC_EnableIRQ(SERCOM1_1_IRQn); // txc ?
  NVIC_EnableIRQ(SERCOM1_0_IRQn); // tx, 
	// set baud 
  UBD_SER_USART.BAUD.reg = UBD_BAUD_VAL;
  // and finally, a kickoff
  while(UBD_SER_USART.SYNCBUSY.bit.ENABLE);
  UBD_SER_USART.CTRLA.bit.ENABLE = 1;
  // enable rx interrupt 
  UBD_SER_USART.INTENSET.bit.RXC = 1;
}

void SERCOM1_2_Handler(void){
  //ERRLIGHT_TOGGLE;
	ucBusDrop->rxISR();
}

void UCBus_Drop::rxISR(void){
  DEBUG2PIN_ON;
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
  uint8_t data = UBD_SER_USART.DATA.reg;
  // for now, just running the clk on every word 
  if((data >> 7) == 0){ // timer bit, on every 0th bit, and beginning of word 
    inWord[0] = data;
    timeTick ++;
    timeBlink ++;
    if(timeBlink >= blinkTime){
      CLKLIGHT_TOGGLE;
      timeBlink = 0;
    }
    onRxISR(); // on start of each word 
  } else { // end of word on every 0th bit 
    inWord[1] = data;
    // now decouple, 
    inHeader = ((inWord[0] >> 1) & 0b00111000) | ((inWord[1] >> 4) & 0b00000111);
    inByte = ((inWord[0] << 4) & 0b11110000) | (inWord[1] & 0b00001111);
    // now check incoming data, 
    // could speed this up to not re-evaluate (inHeader & 0b00110000) every time 
    if((inHeader & 0b00110000) == 0b00100000){ // has-token, CHA
      lastWordAHadToken = true;
      if(inBufferALen != 0){ // in this case, we didn't read-out of the buffer in time, 
        inBufferALen = 0;    // so we reset it, missing the last packet !
        inBufferAWp = 0;
      }
      inBufferA[inBufferAWp] = inByte;
      inBufferAWp ++;
    } else if ((inHeader & 0b00110000) == 0b00000000) { // no-token, CHA
      if(lastWordAHadToken){
        inBufferALen = inBufferAWp;
        onPacketARx();
      }
      lastWordAHadToken = false;
    } else if ((inHeader & 0b00110000) == 0b00110000) { // has-token, CHB 
      //DEBUG1PIN_ON;
      lastWordBHadToken = true;
      if(inBufferBLen != 0){
        inBufferBLen = 0;
      }
      inBufferB[inBufferBWp] = inByte;
      inBufferBWp ++;
    } else if ((inHeader & 0b00110000) == 0b00010000) { // no-token, CHB
      if(lastWordBHadToken){ // falling edge, packet delineation 
        inBufferBLen = inBufferBWp;
        if(inBufferB[0] == id){ //check if pck is for us and update reciprocal buffer len 
          rcrxb = inBufferB[1];
          //lastrc = millis(); 
        } else {  // packet is not ours, ignore, ready for next read 
          inBufferBWp = 0;
          inBufferBLen = 0;
        }
      }
      lastWordBHadToken = false;
    }
    // now check if outgoing tick is ours, 
    // possible bugfarm: if we (potentially) fire the onPacketARx() handler up there, 
    // and then return here to also do all of this work, we might have a l e n g t h y interrupt 
    if((inHeader & dropIdMask) == id){
      // this drop is currently 'tapped' - if it's token less, the data byte was rcrxb for us 
      if(!(inHeader & 0b00100000)){
        rcrxb = inByte;
        //lastrc = millis();
      }
      // our transmit 
      if(outBufferLen > 0){
        // ongoing / starting transmit of bytes from our outbuffer onto the line, 
        outByte = outBuffer[outBufferRp];
        outHeader = headerMask & (tokenWord | (id & 0b00011111));
      } else {
        // whenever free space on the line, transmit our recieve buffer # spaces available 
        if(inBufferBLen == 0 && inBufferBWp == 0){
          outByte = 1;  // have clear space available, communicate that to partner 
        } else {
          outByte = 0;  // currently receiving or have packet awaiting handling 
        }
        outHeader = headerMask & (noTokenWord | (id & 0b00011111));
      }
      outWord[0] = 0b00000000 | ((outHeader << 1) & 0b01110000) | (outByte >> 4);
      outWord[1] = 0b10000000 | ((outHeader << 4) & 0b01110000) | (outByte & 0b00001111);
      // now transmit, 
      UBD_DRIVER_ENABLE;
      UBD_SER_USART.DATA.reg = outWord[0];
      UBD_SER_USART.INTENSET.bit.DRE = 1;
      // now do this, 
      if(outBufferLen > 0){
        outBufferRp ++;
        if(outBufferRp >= outBufferLen){
          outBufferRp = 0;
          outBufferLen = 0;
        }
      }
    } else if ((inHeader & dropIdMask) == UBD_CLOCKRESET){
      // reset 
      timeTick = 0;
    }
  } // end 1th bit case, 
  DEBUG2PIN_OFF;
} // end rx-isr 

void SERCOM1_0_Handler(void){
	ucBusDrop->dreISR();
}

void UCBus_Drop::dreISR(void){
  UBD_SER_USART.DATA.reg = outWord[1]; // tx the next word,
  UBD_SER_USART.INTENCLR.reg = SERCOM_USART_INTENCLR_DRE; // clear this interrupt
  UBD_SER_USART.INTENSET.reg = SERCOM_USART_INTENSET_TXC; // now watch transmit complete
}

void SERCOM1_1_Handler(void){
  ucBusDrop->txcISR();
}

void UCBus_Drop::txcISR(void){
  UBD_SER_USART.INTFLAG.bit.TXC = 1; // clear the flag by writing 1 
  UBD_SER_USART.INTENCLR.reg = SERCOM_USART_INTENCLR_TXC; // turn off the interrupt 
  UBD_DRIVER_DISABLE; // turn off the driver, 
}

// -------------------------------------------------------- ASYNC API

boolean UCBus_Drop::ctr_a(void){
  if(inBufferALen > 0){
    return true;
  } else {
    return false;
  }
}

boolean UCBus_Drop::ctr_b(void){
  if(inBufferBLen > 0){
    return true;
  } else {
    return false;
  }
}

size_t UCBus_Drop::read_a(uint8_t *dest){
	if(!ctr_a()) return 0;
  NVIC_DisableIRQ(SERCOM1_2_IRQn);
  // copy out, irq disabled, TODO safety on missing an interrupt in this case?? clock jitter? 
  memcpy(dest, inBufferA, inBufferALen);
  size_t len = inBufferALen;
  inBufferALen = 0;
  inBufferAWp = 0;
  NVIC_EnableIRQ(SERCOM1_2_IRQn);
  return len;
}

size_t UCBus_Drop::read_b(uint8_t *dest){
  if(!ctr_b()) return 0;
  NVIC_DisableIRQ(SERCOM1_2_IRQn);
  // bytes 0 and 1 are the ID and rcrxb, respectively, so app. is concerned with the rest 
  size_t len = inBufferBLen - 2;
  memcpy(dest, &inBufferB[2], len);
  inBufferBLen = 0;
  inBufferBWp = 0;
  NVIC_EnableIRQ(SERCOM1_2_IRQn);
  return len;
}

boolean UCBus_Drop::cts(void){
  if(outBufferLen > 0 || rcrxb == 0){
    return false;
  } else {
    return true;
  }
}

void UCBus_Drop::transmit(uint8_t *data, uint16_t len){
  if(!cts()) return;
  // 1st byte: num of spaces we have to rx messages at this drop, i.e. buffer space 
  if(inBufferBLen == 0 && inBufferBWp == 0){ // nothing currently reading in, nothing awaiting handle 
    outBuffer[0] = 1;
  } else { // are either currently recieving, or have recieved an unhandled packet 
    outBuffer[1] = 0;
  }
  // also decriment our accounting of their rcrxb
  rcrxb -= 1;
  memcpy(&outBuffer[1], data, len);
  outBufferLen = len + 1; // + 1 for the buffer space 
  outBufferRp = 0;
}

#endif 