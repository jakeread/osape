// DIPs

#include "ucBusDipConfig.h"

void dip_setup(void){
    // set direction in,
    DIP_PORT.DIRCLR.reg = D_BM(D0_PIN) | D_BM(D1_PIN) | D_BM(D2_PIN) | D_BM(D3_PIN) | D_BM(D4_PIN) | D_BM(D5_PIN) | D_BM(D6_PIN) | D_BM(D7_PIN);
    // enable in,
    DIP_PORT.PINCFG[D0_PIN].bit.INEN = 1;
    DIP_PORT.PINCFG[D1_PIN].bit.INEN = 1;
    DIP_PORT.PINCFG[D2_PIN].bit.INEN = 1;
    DIP_PORT.PINCFG[D3_PIN].bit.INEN = 1;
    DIP_PORT.PINCFG[D4_PIN].bit.INEN = 1;
    DIP_PORT.PINCFG[D5_PIN].bit.INEN = 1;
    DIP_PORT.PINCFG[D6_PIN].bit.INEN = 1;
    DIP_PORT.PINCFG[D7_PIN].bit.INEN = 1;
    // enable pull,
    DIP_PORT.PINCFG[D0_PIN].bit.PULLEN = 1;
    DIP_PORT.PINCFG[D1_PIN].bit.PULLEN = 1;
    DIP_PORT.PINCFG[D2_PIN].bit.PULLEN = 1;
    DIP_PORT.PINCFG[D3_PIN].bit.PULLEN = 1;
    DIP_PORT.PINCFG[D4_PIN].bit.PULLEN = 1;
    DIP_PORT.PINCFG[D5_PIN].bit.PULLEN = 1;
    DIP_PORT.PINCFG[D6_PIN].bit.PULLEN = 1;
    DIP_PORT.PINCFG[D7_PIN].bit.PULLEN = 1;
    // 'pull' references the value set in the 'out' register, so to pulldown:
    DIP_PORT.OUTCLR.reg = D_BM(D0_PIN) | D_BM(D1_PIN) | D_BM(D2_PIN) | D_BM(D3_PIN) | D_BM(D4_PIN) | D_BM(D5_PIN) | D_BM(D6_PIN) | D_BM(D7_PIN);
}

uint8_t dip_readLowerFive(void){
    uint32_t bits[5] = {0,0,0,0,0};
    if(DIP_PORT.IN.reg & D_BM(D7_PIN)) { bits[0] = 1; }
    if(DIP_PORT.IN.reg & D_BM(D6_PIN)) { bits[1] = 1; }
    if(DIP_PORT.IN.reg & D_BM(D5_PIN)) { bits[2] = 1; }
    if(DIP_PORT.IN.reg & D_BM(D4_PIN)) { bits[3] = 1; }
    if(DIP_PORT.IN.reg & D_BM(D3_PIN)) { bits[4] = 1; }
    /*
    bits[0] = (DIP_PORT.IN.reg & D_BM(D7_PIN)) >> D7_PIN;
    bits[1] = (DIP_PORT.IN.reg & D_BM(D6_PIN)) >> D6_PIN;
    bits[2] = (DIP_PORT.IN.reg & D_BM(D5_PIN)) >> D5_PIN;
    bits[3] = (DIP_PORT.IN.reg & D_BM(D4_PIN)) >> D4_PIN;
    bits[4] = (DIP_PORT.IN.reg & D_BM(D3_PIN)) >> D3_PIN;
    */
    // not sure why I wrote this as uint32 (?) 
    uint32_t word = 0;
    word = word | (bits[4] << 4) | (bits[3] << 3) | (bits[2] << 2) | (bits[1] << 1) | (bits[0] << 0);
    return (uint8_t)word;
}

boolean dip_readPin0(void){
    return DIP_PORT.IN.reg & D_BM(D0_PIN);
}

boolean dip_readPin1(void){
    return DIP_PORT.IN.reg & D_BM(D1_PIN);
}