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
    _vPorts[_numVPorts ++] = vPort;
    return true;
  }
}

// packet to read from, response to write into, write pointer, maximum response length
// assumes header won't be longer than received max seg length, if it arrived ...
// ptr = DK_x, ptr - 5 = PK_DEST, ptr - 6 = PK_PTR
// effectively the reverse-route routine. 
// bit of a wreck, could do better to separate reverseRoute algorithm & others, 
boolean OSAP::formatResponseHeader(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, uint16_t checksum, VPort *vp, uint16_t vpi){
  // sanity check, this should be pingreq key
  // sysError("FRH pck[ptr]: " + String(pck[ptr]));
  // buf[(*ptr) ++] = val & 255
  // pck like:
  //             [rptr]      [rend]                                     [ptr]
  // [77:3][dep0][e1][e2][e3][pk_ptr][pk_dest][acksegsize:2][checksum:2][dkey-req]
  // response (will be) like:
  //                                                                    [wptr]
  //                                                                    [ptr]
  // [77:3][dep0][pk_ptr][p3][p2][p1][pk_dest][acksegsize:2][checksum:2][dkey-res]
  // ptr here will always indicate end of the header,
  // leaves space until pck[3] for the 77-ack, which will write in later on,
  // to do this, we read forwarding steps from e1 (incrementing read-ptr)
  // and write in tail-to-head, (decrementing write ptr)
  #warning this probably breaks when reversing a route w/ some bus fwd in it 
  uint16_t wptr = ptr - 5; // to beginning of dest, segsize, checksum block
  _res[wptr ++] = PK_DEST;
  ts_writeUint16(segsize, _res, &wptr);
  ts_writeUint16(checksum, _res, &wptr);
  wptr -= 5; // back to start of this block,
  // now find rptr beginning,
  uint16_t rptr = 0; // departure port was start of pck 
  switch(pck[rptr]){ // walk to e1, ignoring original departure information
    case PK_PORTF_KEY:
      rptr += PK_PORTF_INC;
      break;
    case PK_BUSF_KEY:
      rptr += PK_BUSF_INC;
      break;
    case PK_BUSB_KEY:
      rptr += PK_BUSB_INC;
      break;
    default:
      sysError("nonrecognized departure key on reverse route, bailing " + String(pck[rptr]));
      return false;
  }
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
  uint16_t pl = 0;
  uint8_t pwp = 0;
  uint8_t drop = 0;
  unsigned long pat = 0;  
  unsigned long now = millis();
  // loop over vports 
  VPort* vp;              // vport 
  for(uint8_t p = 0; p < _numVPorts; p ++){
    vp = _vPorts[p];
    vp->loop(); // affordance to run phy code,
    for(uint8_t t = 0; t < 4; t ++){ // handles 4 packets per port per turn 
      // reset counters, check for packet 
      pl = 0;
      ptr = 0;
      switch(vp->portTypeKey){
        case EP_PORTTYPEKEY_DUPLEX:
          vp->read(&pck, &pl, &pwp, &pat);
          break;
        case EP_PORTTYPEKEY_BUSHEAD:
        case EP_PORTTYPEKEY_BUSDROP:
          ERRLIGHT_TOGGLE;
          // read, and learn which bus addr transmitted 
          vp->read(&pck, &pl, &pwp, &pat, &drop);
          break;
        default:
          sysError("broken vport, bad type");
          return;
      }
      // if we have a packet, do stuff 
      if(pl > 0){
        // check prune stale, 
        if(pat + OSAP_STALETIMEOUT < now){
          sysError("stale pck on " + String(p));
          vp->clear(pwp);
          continue; // next packet in port 
        }
        // walk through for instruction ptr (max 16-hop packet then)
        #warning another walk that likely breaks w/ bus-fwd 86 and 87 
        for(uint8_t i = 0; i < 16; i ++){
          switch(pck[ptr]){
            case PK_PTR:
              instructionSwitch(pck, pl, ptr + 1, vp, p, pwp);
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
              vp->clear(pwp);
              goto endWalk;
            default:
              sysError("bad walk for ptr: key " + String(pck[ptr]) + " at: " + String(ptr));
              vp->clear(pwp);
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