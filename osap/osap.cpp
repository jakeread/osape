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

void OSAP::portforward(uint8_t* pck, uint16_t pl, uint16_t ptr, VPort* avp, uint8_t avpi, uint8_t pwp, VPort* fwvp, uint8_t fwvpi){
  // have correct VPort type, 
  // check if forwarding is clear, 
  if(!fwvp->cts()){
    if(fwvp->status() != EP_PORTSTATUS_OPEN){ avp->clear(pwp); }
    return;
  }
  sysError("portf");
  // fwd... and check js, which finds the 88 (ptr) where it expects instruction ? 
  // ready, drop arrival information in 
  // have currently 
  //      [pck[ptr]]
  // [ptr][portf_departure][b0][b1]
  // do
  // [portf_arrival][b0][b1][ptr]
  pck[ptr - 1] = PK_PORTF_KEY;
  ts_writeUint16(avpi, pck, &ptr);
  pck[ptr] = PK_PTR;
  // now we can transmit 
  fwvp->send(pck, pl);
  avp->clear(pwp);
}

// pck[ptr] == busf / busb 
void OSAP::busforward(uint8_t* pck, uint16_t pl, uint16_t ptr, VPort* avp, uint8_t avpi, uint8_t pwp, VPort* fwvp, uint8_t fwvpi){
  DEBUG2PIN_TOGGLE;
  //                  [ptr]
  // advance [ptr(88)][busf][b0_depart][b1_depart][b0_rxaddr][b1_rxaddr]
  ptr += 3;
  uint16_t busRxAddr;
  ts_readUint16(&busRxAddr, pck, &ptr); // advances ptr by 2 
  // now, have busRxAddr (drop of the bus we're forwarding to)
  // sysError("fwd to bus addr " + String(busRxAddr));
  // check that bus-drop is not fwding to anything other than bus-head 
  if(fwvp->portTypeKey == EP_PORTTYPEKEY_BUSDROP && busRxAddr != 0){
    sysError("cannot fwd from drop to drop, must pass thru bus head");
    avp->clear(pwp);
    return;
  }
  // check clear,
  if(!(fwvp->cts(busRxAddr))){ 
    if(fwvp->status() != EP_PORTSTATUS_OPEN){ avp->clear(pwp); } // pop on closed ports 
    return; 
  }
  // ready to fwd, bring ptr back so that pck[ptr] == busf 
  ptr -= 5;
  //      [pck[ptr]]
  // [ptr][busf_departure][b0][b1][b2][b3]
  if(avp->portTypeKey == EP_PORTTYPEKEY_DUPLEX){
    // do
    // [portf_arrival][b0][b1][?][?][ptr]
    pck[ptr - 1] = PK_PORTF_KEY;
    ts_writeUint16(avpi, pck, &ptr);
    pck[ptr ++] = PK_BUS_FWD_SPACE_2;
    pck[ptr ++] = PK_BUS_FWD_SPACE_1;
  } else {
    // do
    // [busf_arrival][b0][b1][b2][b3][ptr]
    pck[ptr - 1] = PK_BUSF_KEY;
    // exclude this for now, can add in later, at the moment bus-to-bus fwd unlikely 
    #warning no bus-to-bus fwd code yet 
    sysError("bus to bus forwarding currently unsupported");
    avp->clear(pwp);
    return;
    //ts_writeUint16(avp->getTransmitterDropForPack(pwp), pck, &ptr);
    //ts_writeUint16(avp->getDropId(), pck, &ptr);
  }
  pck[ptr] = PK_PTR; // in front of next instruction 
  // pck is setup, can forward
  fwvp->send(pck, pl, busRxAddr);
  avp->clear(pwp);
}

// pck[ptr] == PK_PORTF_KEY or PK_BUSF_KEY or PK_BUSB_KEY 
void OSAP::forward(uint8_t *pck, uint16_t pl, uint16_t ptr, VPort *vp, uint8_t vpi, uint8_t pwp){
  // vport to fwd on 
  uint8_t fwtype = pck[ptr ++]; // get at ptr, *then* increment ptr 
  uint16_t fwvpi;
  ts_readUint16(&fwvpi, pck, &ptr); // ptr now at bytes succeeding the ptr, read increments 
  // reset ptr so pck[ptr] == pk_porf_key / pk_busf_key etc, for next fwding functions 
  ptr -= 3;
  // find the vport, 
  if(_numVPorts <= fwvpi){ // doesn't exist 
    sysError("during fwd, no vport here at indice " + String(fwvpi));
    vp->clear(pwp);
    return;
  }
  // pull it, do stuff 
  VPort *fwvp = _vPorts[fwvpi];
  // check port type suits forward type
  switch(fwtype){
    case PK_PORTF_KEY:
      if(fwvp->portTypeKey != EP_PORTTYPEKEY_DUPLEX){
        vp->clear(pwp);
        return;
      } else {
        portforward(pck, pl, ptr, vp, vpi, pwp, fwvp, fwvpi);
      }
      break;
    case PK_BUSF_KEY:
      if(fwvp->portTypeKey != EP_PORTTYPEKEY_BUSHEAD && fwvp->portTypeKey != EP_PORTTYPEKEY_BUSDROP){
        vp->clear(pwp);
        return;
      } else {
        busforward(pck, pl, ptr, vp, vpi, pwp, fwvp, fwvpi);
      }
      break;
    case PK_BUSB_KEY:
      if(fwvp->portTypeKey != EP_PORTTYPEKEY_BUSHEAD){
        vp->clear(pwp);
        return;
      } else {
        // would do this here, but 
        // also... bug... this sysError message doesn't escape? que? 
        sysError("no support for osap broadcasts yet");
        vp->clear(pwp);
        return;
      }
      break;
    default:
      sysError("bad fwd type key: " + String(fwtype));
      vp->clear(pwp);
      return;
  }
} // end forward 

// frame: the buffer, ptr: the location of the ptr (ack or pack),
// vp: port received on, fwp: frame-write-ptr,
// so vp->frames[fwp] = frame, though that isn't exposed here
void OSAP::instructionSwitch(uint8_t *pck, uint16_t pl, uint16_t ptr, VPort *vp, uint8_t vpi, uint8_t pwp){
  DEBUG1PIN_TOGGLE;
  // we must *do* something, and (ideally) pop this thing,
  switch(pck[ptr]){
    case PK_PORTF_KEY:
    case PK_BUSF_KEY:
    case PK_BUSB_KEY:
      forward(pck, pl, ptr, vp, vpi, pwp);
      break;
    case PK_DEST: {
        ptr ++; // walk past dest key,
        uint16_t segsize = 0;
        uint16_t checksum = 0;
        ts_readUint16(&segsize, pck, &ptr);
        ts_readUint16(&checksum, pck, &ptr);
        if(checksum != pl - ptr){
          sysError("bad checksum, count: " + String(pl - ptr) + " checksum: " + String(checksum));
          vp->clear(pwp);
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
      vp->clear(pwp);
      break;
  }
}

void OSAP::loop(){
  // temporary items, as e check packets 
  uint8_t* pck;
  uint16_t ptr = 0;
  unsigned long now = millis();
  // loop over vports 
  VPort* vp;              // vport 
  for(uint8_t p = 0; p < _numVPorts; p ++){
    vp = _vPorts[p];
    vp->loop(); // affordance to run phy code,
    for(uint8_t t = 0; t < 4; t ++){ // handles 4 packets per port per turn 
      // reset counters, check for packet 
      ptr = 0;
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
        #warning another walk that likely breaks w/ bus-fwd 86 and 87 
        for(uint8_t i = 0; i < 16; i ++){
          switch(pck[ptr]){
            case PK_PTR:
              //instructionSwitch(pck, &pckm, ptr + 1);
              goto endWalk;
            case PK_PORTF_KEY: // previous instructions, walk over,
              ptr += PK_PORTF_INC;
              break;
            case PK_BUS_FWD_SPACE_1:
              ptr += 1;
              break;
            case PK_BUS_FWD_SPACE_2:
              ptr += 2;
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
              // ok, lawd, now the improved ptr walk for 86, 87 keys. should get us to the instruction switch, 
              // then to the ring.
              // will cut up the exhaust now, then return here in the cold. 
              sysError("bad walk for ptr: key " + String(pck[ptr]) + " at: " + String(ptr));
              pckm.vpa->clear(pckm.location);
              goto endWalk;
          } // end switch
        } // end loop for ptr walk,
      } else {
        break;
      } // no frames in this port,
      // end of this-port-scan,
      endWalk: ;
    } // end packets-per-port-per-turn 
  } // end loop over ports (handling rx) 
}