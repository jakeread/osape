/*
osap/osap.cpp

virtual network node

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "osap.h"

OSAP::OSAP(String nodeName){
  name = nodeName;
}

boolean OSAP::addVPort(VPort* vPort){
  if(_numVPorts > OSAP_MAX_VPORTS){
    return false;
  } else {
    vPort->init();
    vPort->indice = _numVPorts;
    _vPorts[_numVPorts ++] = vPort;
    return true;
  }
}

// packet to read from, response to write into (global _res)
// apparent position of pck[ptr] == DK_(?), maximum response length, checksum to write, 
// vport rx'd on, indice of vport rx'd on
// this reverses the route, to transmit back on the vport rx'd on
// and writes the checksum, seglength, but not the data 
boolean OSAP::formatResponseHeader(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, uint16_t checksum, VPort *vp, uint16_t vpi){
  // when called, pck[ptr] == 1st byte of datagram (destination key)
  // read pointer *will* become where we read from, 
  uint16_t rptr = ptr - 5;  // now pck[wptr] == PK_DEST, pck[wptr + 1] starts the segsize, pck[wptr + 3] the checksum 
  _res[rptr ++] = PK_DEST;  // re-assert pck[wptr] == PK_DEST 
  ts_writeUint16(segsize, _res, &rptr); // write in route segsize 
  ts_writeUint16(checksum, _res, &rptr);  // write in response checksum 
  // so we have currently 
  //                                                 [pck[ptr]]
  // [departure_0][arrival_1][arrival_...][arrival_n][pk_dest][segsize:2][checksum:2][payload]
  // prep to reverse packet, 
  rptr -= 5; // return to the start of that block, pck[wptr] == PK_DEST again 
  uint16_t wptr = 0; // write pointer, from the beginning 
  uint16_t rend = rptr; // if we pass this while reading, past end of route head 
  // now we write our departure port, 
  switch(vp->portTypeKey){
    case EP_PORTTYPEKEY_DUPLEX:
      pck[wptr ++] = PK_PORTF_KEY;
      ts_writeUint16(vpi, pck, &wptr);
      break;
    case EP_PORTTYPEKEY_BUSHEAD:
    case EP_PORTTYPEKEY_BUSDROP:
      pck[wptr ++] = PK_BUSF_KEY;
      ts_writeUint16(vpi, pck, &wptr);
      ts_writeUint16(vp->getTransmitterDropForPck(pwp));
      break;
    default:
      vp->clear(pwp);
      return;
  }
  #warning this is where you left off, above, getTransmitterDrop ... etc 
  // considering the above: requirement to carry thru now *also* the 
  // transmitter drop, etc, the move might be to 
  // just write these things into the pck on arrival, 
  // *then* handle them, then all of these fn's just read out of the pck again 
  // go know what-all is going on... time saved is probably negligible from carrying 
  // all of this stuff thru, and it's a hot mess, hugely stateful. micros are strong now. 

  // end switch, now pck[rptr] is at port-type-key of next fwd instruction
  // walk rptr forwards, wptr backwards, copying in forwarding segments, max. 16 hops
  uint16_t rend = ptr - 6;  // read-end per static pck-at-dest end block: 6 for checksum(2) acksegsize(2), dest and ptr
  for(uint8_t h = 0; h < 16; h ++){
    if(rptr >= rend){ // terminate when walking past end,
      break;
    }
    switch(pck[rptr]){
      case PK_PORTF_KEY:
        wptr -= PK_PORTF_INC;
        for(uint8_t i = 0; i < PK_PORTF_INC; i ++){
          _res[wptr + i] = pck[rptr ++];
        }
        break;
      case PK_BUSF_KEY:
      case PK_BUSB_KEY:
        wptr -= PK_BUSF_INC;
        for(uint8_t i = 0; i < PK_BUSF_INC; i ++){
          _res[wptr + i] = pck[rptr ++];
        }
      default:
        sysError("rptr: " + String(rptr) + " key here: " + String(pck[rptr]));
        sysError("couldn't reverse this path");
        return false;
    }
  }
  // following route-copy-in,
  // TODO mod this for busses,
  #warning no bus reversal here 
  wptr -= 4;
  _res[wptr ++] = PK_PORTF_KEY; /// write in departure key type,
  ts_writeUint16(vpi, _res, &wptr); // write in departure port,
  _res[wptr ++] = PK_PTR; // ptr follows departure key,
  // to check, wptr should now be at 4: for departure(3:portf), ptr(1)
  if(wptr != 4){ // wptr != 7
    sysError("bad response header write");
    return false;
  } else {
    return true;
  }
}

// write and send ahn reply out, 
void OSAP::appReply(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, VPort* vp, uint16_t vpi, uint8_t *reply, uint16_t rl){
  uint16_t wptr = ptr;
  for(uint16_t i = 0; i < rl; i ++){
    _res[wptr ++] = reply[i];
  }
  if(formatResponseHeader(pck, pl, ptr, segsize, rl, vp, vpi)){
    vp->send(_res, wptr);
  } else {
    sysError("bad response format");
  }
}

void OSAP::portforward(uint8_t* pck, uint16_t ptr, pckm_t* pckm, VPort* fwvp){
  // have correct VPort type, 
  // check if forwarding is clear, 
  if(!fwvp->cts()){
    if(fwvp->status() != EP_PORTSTATUS_OPEN){ pckm->vpa->clear(pckm->location); }
    return;
  }
  // are clear to send this, so 
  // ready to drop arrival information in, 
  // currently pck[ptr] == [portf]
  if(fwvp->portTypeKey == EP_PORTTYPEKEY_DUPLEX){
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
  pck[ptr] == PK_PTR;
  // are setup to send on the port, 
  fwvp->send(pck, pckm->len);
  pckm->vpa->clear(pckm->location);
}

// pck[ptr] == busf / busb 
void OSAP::busforward(uint8_t* pck, uint16_t ptr, pckm_t* pckm, VPort* fwvp){
  DEBUG2PIN_TOGGLE;
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
    if(fwvp->status() != EP_PORTSTATUS_OPEN){ pckm->vpa->clear(pckm->location); } // pop on closed ports 
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
        pckm->vpa->clear(pckm->location);
        return;
      } else {
        portforward(pck, ptr, pckm, fwvp);
      }
      break;
    case PK_BUSF_KEY:
      if(fwvp->portTypeKey != EP_PORTTYPEKEY_BUSHEAD && fwvp->portTypeKey != EP_PORTTYPEKEY_BUSDROP){
        pckm->vpa->clear(pckm->location);
        return;
      } else {
        busforward(pck, ptr, pckm, fwvp);
      }
      break;
    case PK_BUSB_KEY:
      if(fwvp->portTypeKey != EP_PORTTYPEKEY_BUSHEAD){
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

// frame: the buffer, ptr: the location of the ptr (ack or pack),
// vp: port received on, fwp: frame-write-ptr,
// so vp->frames[fwp] = frame, though that isn't exposed here
void OSAP::instructionSwitch(uint8_t *pck,uint16_t ptr, pckm_t* pckm){
  DEBUG1PIN_TOGGLE;
  // we must *do* something, and (ideally) pop this thing,
  switch(pck[ptr]){
    case PK_PORTF_KEY:
    case PK_BUSF_KEY:
    case PK_BUSB_KEY:
      forward(pck, ptr, pckm);
      break;
    case PK_DEST: { // it's us, do checksum etc... 
        uint16_t segsize = 0;
        uint16_t checksum = 0;
        ptr ++;     // walk past dest key,
        ts_readUint16(&segsize, pck, &ptr);
        ts_readUint16(&checksum, pck, &ptr);
        if(checksum != pckm->len - ptr){
          sysError("bad checksum, count: " + String(pckm->len - ptr) + " checksum: " + String(checksum));
          pckm->vpa->clear(pckm->location);
        } else {
          switch(pck[ptr]){
            case DK_APP:
              handleAppPacket(pck, pl, ptr, segsize, vp, vpi, pwp);
              break;
            case DK_PINGREQ:
              handlePingRequest(pck, pl, ptr, segsize, vp, vpi, pwp);
              break;
            case DK_RREQ:
              handleReadRequest(pck, pl, ptr, segsize, vp, vpi, pwp);
              break;
              break;
            case DK_WREQ: // no writing yet,
            case DK_PINGRES: // not issuing pings from embedded, shouldn't have deal w/ their responses
            case DK_RRES: // not issuing requests from embedded, same
            case DK_WRES: // not issuing write requests from embedded, again
              sysError("WREQ or RES received in embedded, popping");
              vp->clear(pwp);
              break;
            default:
              sysError("non-recognized destination key, popping");
              vp->clear(pwp);
              break;
          }
        }
      }
      break;
    default:
      // packet is unrecognized,
      sysError("unrecognized instruction key");
      pckm->vpa->clear(pckm->location);
      break;
  }
}

void OSAP::loop(){
  // temporary items, as e check packets 
  unsigned long now = millis();
  // loop over vports 
  VPort* vp;              // vport 
  for(uint8_t p = 0; p < _numVPorts; p ++){
    vp = _vPorts[p];
    vp->loop(); // affordance to run phy code,
    for(uint8_t t = 0; t < 4; t ++){ // handles 4 packets per port per turn 
      // reset counters: the packet, the pointer, and the meta 
      uint8_t* pck;
      uint16_t ptr = 0;
      pckm_t pckm; // consider using one static ptr to pckm, for loop() speedup, if it counts (scope test)
      vp->read(&pck, &pckm);
      // if the packet is length-full, do stuff 
      if(pckm.len > 0){        // check prune stale, 
        if(pckm.at + OSAP_STALETIMEOUT < now){
          sysError("stale pck on " + String(p));
          pckm.vpa->clear(pckm.location);
          continue; // next packet in port 
        }
        // walk through for instruction ptr (max 16-hop packet then)
        for(uint8_t i = 0; i < 16; i ++){
          switch(pck[ptr]){
            case PK_PTR:
              instructionSwitch(pck, ptr + 1, &pckm);
              goto endWalk;
            case PK_PORTF_KEY: // previous instructions, walk over,
              ptr += PK_PORTF_INC;
              break;
            case PK_BUSF_KEY:
              ptr += PK_BUSF_INC;
              break;
            case PK_BUSB_KEY:
              ptr += PK_BUSF_INC;
              break;
            case PK_LLERR:
              // someone forwarded us an err-escape,
              // since this leg is embedded & has no display, just wipe it 
              pckm.vpa->clear(pckm.location);
              goto endWalk;
            default:
              // no dice in the forward walk, clear it 
              sysError("bad walk for ptr: key " + String(pck[ptr]) + " at: " + String(ptr));
              pckm.vpa->clear(pckm.location);
              goto endWalk;
          } // end switch
        } // end loop for ptr walk,
      } else {
        // pck len is zero, meaning this port has no packets,
        break; // break the loop, 
      } // no frames in this port,
      // end of this-port-scan,
      endWalk: ;
    } // end packets-per-port-per-turn 
  } // end loop over ports (handling rx) 
}