/*
osap/osap-forwarding.cpp

virtual network handlers for pass-thru packets 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "osap.h"

void OSAP::portforward(uint8_t* pck, uint16_t ptr, pckm_t* pckm, VPort* fwvp){
  // have correct VPort type, 
  // check if forwarding is clear, 
  if(!fwvp->cts(0)){
    if(fwvp->status(0) != EP_PORTSTATUS_OPEN){ pckm->vpa->clear(pckm->location); }
    return;
  }
  // are clear to send this, so 
  // ready to drop arrival information in, 
  // currently pck[ptr] == [portf]
  if(pckm->vpa->portTypeKey == EP_PORTTYPEKEY_DUPLEX){
    ptr -= 4;                                     
    pck[ptr ++] = PK_PORTF_KEY;                   // reversal instruction is to (1) port forward
    ts_writeUint16(pckm->vpa->indice, pck, &ptr); // (2) on this vport 
  } else {
    ptr -= 6;
    pck[ptr ++] = PK_BUSF_KEY;                    // reversal instruction is to (1) fwd thru a bus 
    ts_writeUint16(pckm->vpa->indice, pck, &ptr); // (2) on this vport
    ts_writeUint16(pckm->txAddr, pck, &ptr);      // (3) towards this bus address 
  }
  // now have to walk the ptr forwards, 
  for(uint8_t i = 0; i < 3; i ++){
    pck[ptr] = pck[ptr + 1];
    ptr ++;
  }
  // new ptr at tail 
  pck[ptr] = PK_PTR;
  // are setup to send on the port, 
  fwvp->send(pck, pckm->len, 0);
  pckm->vpa->clear(pckm->location);
}

// pck[ptr] == busf / busb 
void OSAP::busforward(uint8_t* pck, uint16_t ptr, pckm_t* pckm, VPort* fwvp){
  //DEBUG2PIN_TOGGLE;
  //                  [ptr]
  // advance [ptr(88)][busf][b0_depart][b1_depart][b0_rxaddr][b1_rxaddr]
  ptr += 3;
  uint16_t fwdRxAddr;
  ts_readUint16(&fwdRxAddr, pck, &ptr); // advances ptr by 2 
  // now, have busRxAddr (drop of the bus we're forwarding to)
  // sysError("fwd to bus addr " + String(fwdRxAddr));
  // check that bus-drop is not fwding to anything other than bus-head 
  if(fwvp->portTypeKey == EP_PORTTYPEKEY_BUSDROP && fwdRxAddr != 0){
    sysError("cannot fwd from drop to drop, must pass thru bus head");
    pckm->vpa->clear(pckm->location);
    return;
  }
  // check clear,
  if(!(fwvp->cts(fwdRxAddr))){ 
    if(fwvp->status(fwdRxAddr) != EP_PORTSTATUS_OPEN){ pckm->vpa->clear(pckm->location); } // pop on closed ports 
    return; 
  }
  // ready to fwd, bring ptr back so that pck[ptr] == busf 
  ptr -= 5; // 3 ahead ~ 15 lines above, 2 when ts_read... advances by 2 
  // write in to previous instruction,  
  //      [pck[ptr]]
  // [ptr][busf_departure][b0][b1][b2][b3]
  if(pckm->vpa->portTypeKey == EP_PORTTYPEKEY_DUPLEX){ // arrived on p2p 
    // prev. instruction was portf,
    ptr -= 4; // pck[ptr] == portf key for previous fwd,
    pck[ptr ++] = PK_PORTF_KEY; // re-assert 
    ts_writeUint16(pckm->vpa->indice, pck, &ptr); // write arrival port in 
  } else {
    // prev. instruction was busf or busb,
    ptr -= 6;
    pck[ptr ++] = PK_BUSF_KEY;                    // reversal instruction is to (1) fwd back 
    ts_writeUint16(pckm->vpa->indice, pck, &ptr); // (2) on this vport
    ts_writeUint16(pckm->txAddr, pck, &ptr);      // (3) towards this bus address 
  }
  // now pck[ptr] == PK_PTR, the BUSF instruction is next, 
  // we increment so that our fwd instruction is occupying this space, ptr is following. 
  for(uint8_t i = 0; i < 5; i ++){
    pck[ptr] = pck[ptr + 1];
    ptr ++;
  }
  // finally, ptr in front of next instruction, 
  pck[ptr] = PK_PTR; 
  // pck is setup, can forward
  fwvp->send(pck, pckm->len, fwdRxAddr);
  // and clear (send copies into tx buffer)
  pckm->vpa->clear(pckm->location);
}

// pck[ptr] == PK_PORTF_KEY or PK_BUSF_KEY or PK_BUSB_KEY 
void OSAP::forward(uint8_t *pck, uint16_t ptr, pckm_t* pckm){
  // vport to fwd on 
  uint8_t fwtype = pck[ptr ++]; // get at ptr, *then* increment ptr 
  uint16_t fwvpi;
  ts_readUint16(&fwvpi, pck, &ptr); // ptr now at bytes succeeding the ptr, read increments 
  // reset ptr so pck[ptr] == pk_porf_key / pk_busf_key etc, for next fwding functions 
  ptr -= 3;
  // find the vport, 
  if(_numVPorts <= fwvpi){ // doesn't exist 
    sysError("during fwd, no vport here at indice " + String(fwvpi));
    pckm->vpa->clear(pckm->location);
    return;
  }
  // pull it, do stuff 
  VPort *fwvp = _vPorts[fwvpi];
  // check port type suits forward type
  switch(fwtype){
    case PK_PORTF_KEY:
      if(fwvp->portTypeKey != EP_PORTTYPEKEY_DUPLEX){
        sysError("dropping fwd for portf on non portf exit");
        pckm->vpa->clear(pckm->location);
        return;
      } else {
        portforward(pck, ptr, pckm, fwvp);
      }
      break;
    case PK_BUSF_KEY:
      if(fwvp->portTypeKey != EP_PORTTYPEKEY_BUSHEAD && fwvp->portTypeKey != EP_PORTTYPEKEY_BUSDROP){
        sysError("dropping fwd for busf on non busf exit");
        pckm->vpa->clear(pckm->location);
        return;
      } else {
        busforward(pck, ptr, pckm, fwvp);
      }
      break;
    case PK_BUSB_KEY:
      if(fwvp->portTypeKey != EP_PORTTYPEKEY_BUSHEAD){
        sysError("dropping request to broadcast on non bus-head exit");
        pckm->vpa->clear(pckm->location);
        return;
      } else {
        // would do this here, but 
        // also... bug... this sysError message doesn't escape? que? 
        sysError("no support for osap broadcasts yet");
        pckm->vpa->clear(pckm->location);
        return;
      }
      break;
    default:
      sysError("bad fwd type key: " + String(fwtype));
      pckm->vpa->clear(pckm->location);
      return;
  }
} // end forward 