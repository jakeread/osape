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
volatile uint32_t outFrame = 0;
volatile uint8_t outHeader = 0; 
volatile uint8_t outWord[2] = {0,0};
volatile uint8_t currentDropTap = 0; // drop we are currently 'txing' to / drop that will reply on this cycle

// TX'ing things:
#define HEADER_TX_DROP(drop) (drop & 0b00011111)
#define HEADER_TX_CH(ch) ((ch << 5) & 0b00100000)
#define HEADER_TX_TOKEN(ntx) ((numTx << 6) & 0b11000000)
//#define FRAME_TRACKING (0b00000000010000001000000011000000)
// write frames, 
#define WF_0(header) (uint32_t)(0b00000000 | ((header >> 2) & 0b00111111))
#define WF_1(header, word0) (uint32_t)(0b01000000 | ((header << 4) & 0b00110000) | ((word0 >> 4) & 0b00001111))
#define WF_2(word0, word1) (uint32_t)(0b10000000 | ((word0 << 2) & 0b00111100) | ((word1 >> 6) & 0b00000011))
#define WF_3(word1) (uint32_t)(0b11000000 | (word1 & 0b00111111))

// RX'ing things 
#define HEADER_RX_DROP(data) ((uint8_t)(data >> 24) & 0b00011111)
#define HEADER_RX_TOKEN(data) ((uint8_t)(data >> 29) & 0b00000011)
#define RF(data, i) ((uint8_t)(data >> 8 * i))

volatile uint8_t lastSpareEOP = 0;        // last channel we transmitted spare end-of-packet on

// reciprocal recieve buffer spaces 
#warning should be for 30 drops, right (?) 0 clkreset, 1:31 ids, 
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
  outHeader = HEADER_TX_DROP(currentDropTap);
  // increment that, 
  currentDropTap ++;
  if(currentDropTap > 31){
    currentDropTap = 0;
  }

  // ------------------------------------------------------ TX CHA
  if(outBufferALen > 0){
    DEBUG2PIN_HI;
    // find / fill words: 
    uint8_t numTx = outBufferALen - outBufferARp;
    if(numTx > 2) numTx = 2;
    // write header, tx is from cha, and has this count... 
    outHeader |= HEADER_TX_TOKEN(numTx) | HEADER_TX_CH(0);
    //fill bytes accordingly, 
    for(uint8_t i = 0; i < numTx; i ++){
      outWord[i] = outBufferA[outBufferARp ++];
    }
    // check / increment posn w/r/t buffer: if numTx < 2, packet will terminate this frame, can reset 
    if(numTx < 2){
      outBufferALen = 0;
      outBufferARp = 0;
    }
  } else if (outBufferBLen > 0){  // ---------------------- TX CHB 
    // tx from chb
    // find / fill words 
    uint8_t numTx = outBufferBLen - outBufferBRp;
    if(numTx > 2) numTx = 2;
    // write header: tx from chb, has this count 
    outHeader |= HEADER_TX_TOKEN(numTx) | HEADER_TX_CH(1);
    // fill bytes, 
    for(uint8_t i = 0; i < numTx; i ++){
      outWord[i] = outBufferB[outBufferBRp];
    }
    // check / increment posn w/r/t buffer, if numTx < 2, packet terminates this frame 
    if(numTx < 2){
      outBufferBLen = 0;
      outBufferBRp = 0;
    }
  } else {
    // nothing to tx, do rcrxb to that drop (?) 
    if(inBufferLen[currentDropTap] == 0 && inBufferWp[currentDropTap] == 0){
      outWord[0] = 1;
    } else {
      outWord[0] = 0;
    }
    // alternate spare closures on cha / chb 
    if(lastSpareEOP == 0){
      outHeader |= HEADER_TX_CH(1);
      lastSpareEOP = 1;
    } else {
      outHeader |= HEADER_TX_CH(0);
      lastSpareEOP = 0;
    }
    DEBUG2PIN_LO;
  }
  // finally, do the action 
  outFrame = (WF_0(outHeader) << 24) | (WF_1(outHeader, outWord[0])) << 16 | WF_2(outWord[0], outWord[1]) << 8 | WF_3(outWord[1]);
  UBH_SER_USART.DATA.reg = outFrame;
  DEBUG3PIN_LO;
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
  // retrieve header, 
  uint8_t numRx = HEADER_RX_TOKEN(data); // numRx constrained to 3 via 0b00000011
  uint8_t dropRx = HEADER_RX_DROP(data);

  // escape fake drops, 
  if(dropRx > UBH_DROP_OPS || dropRx == 0) return;

  // write in to this drop's buffer, 
  for(uint8_t i = 0; i < numRx; i ++){
      inBuffer[dropRx][inBufferWp[dropRx] + i] = RF(data, i); 
    }
  inBufferWp[dropRx] += numRx;

  // switch on numRx,
  switch(numRx){
    case 3:
      inLastHadToken[dropRx] = true;
      break;
    case 2:
    case 1:
      inLastHadToken[dropRx] = true; // switch through, 
    case 0: // packet edge, meaning *byte 3 here, always no-token if we are in this switch, is rcrxb*
      rcrxb[dropRx] = RF(data, 2);
      if(inLastHadToken[dropRx]){
        // 1st byte is this drop's rcrxb,
        inBufferLen[dropRx] = inBufferWp[dropRx];
      }
      break;
  }
}

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
  // byte 1 is the drop's rcrxb transmitted with this packet
  size_t len = inBufferLen[drop] - 1;
  memcpy(dest, &(inBuffer[drop][1]), len);
  __disable_irq();
  inBufferLen[drop] = 0;
  inBufferWp[drop] = 0;
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
  __disable_irq();
  outBufferBLen = len + 2; // + 1 for the drop + 1 for the spaces 
  outBufferBRp = 0;
  __enable_irq();
}

#endif 