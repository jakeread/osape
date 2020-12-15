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

uint8_t ringFrame[1028];

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

void OSAP::write77(uint8_t *pck, VPort *vp){
  uint16_t one = 1;
  pck[0] = PK_PPACK; // the '77'
  uint16_t bufSpace = vp->getBufSpace();
  ts_writeUint16(bufSpace, pck, &one);
  vp->lastRXBufferSpaceTransmitted = bufSpace;
  vp->rxSinceTx = 0;
}

// packet to read from, response to write into, write pointer, maximum response length
// assumes header won't be longer than received max seg length, if it arrived ...
// ptr = DK_x, ptr - 5 = PK_DEST, ptr - 6 = PK_PTR
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
  uint16_t wptr = ptr - 5; // to beginning of dest, segsize, checksum block
  _res[wptr ++] = PK_DEST;
  ts_writeUint16(segsize, _res, &wptr);
  ts_writeUint16(checksum, _res, &wptr);
  wptr -= 5; // back to start of this block,
  // now find rptr beginning,
  uint16_t rptr = 3; // departure port was trailing 77,
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
      sysError("nonreq departure key on reverse route, bailing");
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
  wptr -= 4;
  _res[wptr ++] = PK_PORTF_KEY; /// write in departure key type,
  ts_writeUint16(vpi, _res, &wptr); // write in departure port,
  _res[wptr ++] = PK_PTR; // ptr follows departure key,
  // to check, wptr should now be at 7: for 77(3), departure(3:portf), ptr(1)
  if(wptr != 7){ // wptr != 7
    sysError("bad response header write");
    return false;
  } else {
    return true;
  }
}

/*
await osap.query(nextRoute, 'name', 'numVPorts')
await osap.query(nextRoute, 'vport', np, 'name', 'portTypeKey', 'portStatus', 'maxSegLength')
*/

void OSAP::writeQueryDown(uint16_t *wptr){
  sysError("QUERYDOWN");
  _res[(*wptr) ++] = EP_ERRKEY;
  _res[(*wptr) ++] = EP_ERRKEY_QUERYDOWN;
}

void OSAP::writeEmpty(uint16_t *wptr){
  sysError("EMPTY");
  _res[(*wptr) ++] = EP_ERRKEY;
  _res[(*wptr) ++] = EP_ERRKEY_EMPTY;
}

// queries for ahn vport,
void OSAP::readRequestVPort(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t rptr, uint16_t *wptr, uint16_t segsize, VPort* vp){
  // could be terminal, could read into endpoints (input / output) of the vport,
  for(uint8_t i = 0; i < 16; i ++){
    if(rptr >= pl){
      return;
    }
    if(*wptr > segsize){
      sysError("QUERYDOWN wptr: " + String(*wptr) + " segsize: " + String(segsize));
      *wptr = ptr;
      writeQueryDown(wptr);
      return;
    }
    switch(pck[rptr]){
      case EP_NUMINPUTS:
        _res[(*wptr) ++] = EP_NUMINPUTS;
        ts_writeUint16(0, _res, wptr); // TODO: vports can have inputs / outputs,
        rptr ++;
        break;
      case EP_NUMOUTPUTS:
        _res[(*wptr) ++] = EP_NUMOUTPUTS;
        ts_writeUint16(0, _res, wptr);
        rptr ++;
        break;
      case EP_INPUT:
      case EP_OUTPUT:
        writeEmpty(wptr); // ATM, these just empty - and then return, further args would be for dive
        return;
      case EP_NAME:
        _res[(*wptr) ++] = EP_NAME;
        ts_writeString(vp->name, _res, wptr);
        rptr ++;
        break;
      case EP_DESCRIPTION:
        _res[(*wptr) ++] = EP_DESCRIPTION;
        ts_writeString(vp->description, _res, wptr);
        rptr ++;
        break;
      case EP_PORTTYPEKEY:
        _res[(*wptr) ++] = EP_PORTTYPEKEY; // TODO for busses
        _res[(*wptr) ++] = vp->portTypeKey;
        rptr ++;
        break;
      case EP_MAXSEGLENGTH:
        _res[(*wptr) ++] = EP_MAXSEGLENGTH;
        ts_writeUint32(vp->maxSegLength, _res, wptr);
        rptr ++;
        break;
      case EP_PORTSTATUS:
        _res[(*wptr) ++] = EP_PORTSTATUS;
        ts_writeBoolean(vp->status, _res, wptr);
        rptr ++;
        break;
      case EP_PORTBUFSPACE:
        _res[(*wptr) ++] = EP_PORTBUFSPACE;
        ts_writeUint16(vp->getBufSpace(), _res, wptr);
        rptr ++;
        break;
      case EP_PORTBUFSIZE:
        _res[(*wptr) ++] = EP_PORTBUFSIZE;
        ts_writeUint16(vp->getBufSize(), _res, wptr);
        rptr ++;
        break;
      default:
        writeEmpty(wptr);
        return;
    }
  }
}

void OSAP::handleReadRequest(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, VPort* vp, uint16_t vpi, uint8_t pwp){
  if(vp->cts()){
    // resp. code,
    // readptr,
    uint16_t rptr = ptr + 1; // this will pass the RREQ and ID bytes, next is first query key
    uint16_t wptr = ptr;
    _res[wptr ++] = DK_RRES;
    _res[wptr ++] = pck[rptr ++];
    _res[wptr ++] = pck[rptr ++];
    // read items,
    // ok, walk those keys
    uint16_t indice = 0;
    for(uint8_t i = 0; i < 16; i ++){
      if(rptr >= pl){
        goto endwalk;
      }
      if(wptr > segsize){
        sysError("QUERYDOWN wptr: " + String(wptr) + " segsize: " + String(segsize));
        wptr = ptr;
        writeQueryDown(&wptr);
        goto endwalk;
      }
      switch(pck[rptr]){
        // first up, handle dives which downselect the tree
        case EP_VPORT:
          rptr ++;
          ts_readUint16(&indice, pck, &rptr);
          if(indice < _numVPorts){
            _res[wptr ++] = EP_VPORT;
            ts_writeUint16(indice, _res, &wptr);
            readRequestVPort(pck, pl, ptr, rptr, &wptr, segsize, _vPorts[indice]);
          } else {
            writeEmpty(&wptr);
          }
          goto endwalk;
        case EP_VMODULE:
          writeEmpty(&wptr);
          goto endwalk;
        // for reading any top-level item,
        case EP_NUMVPORTS:
          _res[wptr ++] = EP_NUMVPORTS;
          ts_writeUint16(_numVPorts, _res, &wptr);
          rptr ++;
          break;
        case EP_NUMVMODULES:
          _res[wptr ++] = EP_NUMVMODULES;
          ts_writeUint16(_numVModules, _res, &wptr);
          rptr ++;
          break;
        case EP_NAME:
          _res[wptr ++] = EP_NAME;
          ts_writeString(name, _res, &wptr);
          rptr ++;
          break;
        case EP_DESCRIPTION:
          _res[wptr ++] = EP_DESCRIPTION;
          ts_writeString(description, _res, &wptr);
          rptr ++;
          break;
        // the default: unclear keys nullify entire response
        default:
          writeEmpty(&wptr);
          goto endwalk;
      } // end 1st switch
    }
    endwalk: ;
    //sysError("QUERY ENDWALK, ptr: " + String(ptr) + " wptr: " + String(wptr));
    if(formatResponseHeader(pck, pl, ptr, segsize, wptr - ptr, vp, vpi)){
      vp->clearPacket(pwp);
      write77(_res, vp);
      vp->sendPacket(_res, wptr);
      vp->decrimentRecipBufSpace();
    } else {
      sysError("bad response format");
      vp->clearPacket(pwp);
    }
  } else {
    vp->clearPacket(pwp);
  }
}

// pck[ptr] == DK_PINGREQ
void OSAP::handlePingRequest(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, VPort* vp, uint16_t vpi, uint8_t pwp){
  if(vp->cts()){ // resp. path is clear, can write resp. and ship it
    // the reversed header will be *the same length* as the received header,
    // which was from 0-ptr! - this is great news, we can say:
    uint16_t wptr = ptr; // start writing here, leaves room for the header,
    _res[wptr ++] = DK_PINGRES; // write in whatever the response is, here just the ping-res key and id
    _res[wptr ++] = pck[ptr + 1];
    _res[wptr ++] = pck[ptr + 2];
    // this'll be the 'std' response formatting codes,
    // formatResponseHeader doesn't need the _res, that's baked in, and it writes 0-ptr,
    // since we wrote into _res following ptr, (header lengths identical), this is safe,
    if(formatResponseHeader(pck, pl, ptr, segsize, 3, vp, vpi)){ // write the header: this goes _resp[3] -> _resp[ptr]
      vp->clearPacket(pwp);                                   // can now rm the packet, have gleaned all we need from it,
      write77(_res, vp);                                 // and *after* rm'ing it, report open space _resp[0] -> _resp[3];
      vp->sendPacket(_res, wptr); // this fn' call should copy-out of our buffer
      vp->decrimentRecipBufSpace();
    } else {
      sysError("bad response format");
      vp->clearPacket(pwp);
    }
  } else {
    vp->clearPacket(pwp);
  }
}

// write and send ahn reply out, 
void OSAP::appReply(uint8_t *pck, uint16_t pl, uint16_t ptr, uint16_t segsize, VPort* vp, uint16_t vpi, uint8_t *reply, uint16_t rl){
  uint16_t wptr = ptr;
  for(uint16_t i = 0; i < rl; i ++){
    _res[wptr ++] = reply[i];
  }
  if(formatResponseHeader(pck, pl, ptr, segsize, rl, vp, vpi)){
    write77(_res, vp);
    vp->sendPacket(_res, wptr);
    vp->decrimentRecipBufSpace();
  } else {
    sysError("bad response format");
  }
}


void OSAP::forward(uint8_t *pck, uint16_t pl, uint16_t ptr, VPort *vp, uint8_t vpi, uint8_t pwp){
  sysError("NO FWD CODE YET");
  vp->clearPacket(pwp);
}

// frame: the buffer, ptr: the location of the ptr (ack or pack),
// vp: port received on, fwp: frame-write-ptr,
// so vp->frames[fwp] = frame, though that isn't exposed here
void OSAP::instructionSwitch(uint8_t *pck, uint16_t pl, uint16_t ptr, VPort *vp, uint8_t vpi, uint8_t pwp){
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
          vp->clearPacket(pwp);
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
              vp->clearPacket(pwp);
              break;
            default:
              sysError("non-recognized destination key, popping");
              vp->clearPacket(pwp);
              break;
          }
        }
      }
      break;
    default:
      // packet is unrecognized,
      sysError("unrecognized instruction key");
      vp->clearPacket(pwp);
      break;
  }
}

void OSAP::loop(){
  /*
  another note 
  this was measured around 25us (long!) 
  so it would be *tite* if that coule be decreased, especially in recognizing the no-op cases, 
  where execution could be very - very - small. 
  */
  unsigned long pat = 0; // packet arrival time 
  VPort* vp;        // vp of vports
  unsigned long now = millis();
  for(uint8_t p = 0; p < _numVPorts; p ++){
    vp = _vPorts[p];
    vp->loop(); // affordance to run phy code,
    for(uint8_t t = 0; t < 4; t ++){ // handles 4 packets per port per turn 
      uint8_t* pck;     // the packet we are handling
      uint16_t pl = 0;  // length of that packet
      uint8_t pwp = 0;  // packet write pointer: where it was (in vp, which space), so that we can instruct vp to clear it later 
      vp->getPacket(&pck, &pl, &pwp, &pat); // gimme the bytes
      if(pl > 0){ // have length, will try,
        // check prune stale, 
        if(pat + OSAP_STALETIMEOUT < now){
          //this untested, but should work, yeah? 
          //sysError("prune stale message on " + String(vp->name));
          vp->clearPacket(pwp);
          continue;
        }
        // check / handle pck
        uint16_t ptr = 0;
        // new rcrbx?
        if(pck[ptr] == PK_PPACK){
          ptr ++;
          uint16_t rcrxs;
          ts_readUint16(&rcrxs, pck, &ptr);
          vp->setRecipRxBufSpace(rcrxs);
        }
        // anything else?
        if(ptr < pl){
          // walk through for instruction,
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
                // we are kind of helpless to help, just escp.
                vp->clearPacket(pwp);
                goto endWalk;
              default:
                sysError("bad walk for ptr: key " + String(pck[ptr]) + " at: " + String(ptr));
                vp->clearPacket(pwp);
                goto endWalk;
            } // end switch
          } // end loop for ptr walk,
        } else {
          // that was just the rcrbx then,
          vp->clearPacket(pwp);
        }
      } else {
        break;
      } // no frames in this port,
      // end of this-port-scan,
      endWalk: ;
    } // end up-to-8-packets-per-turn 
  } // end loop over ports (handling rx) 
  // loop for keepalive conditions, 
  for(uint8_t p = 0; p < _numVPorts; p ++){
    vp = _vPorts[p];
    // check if needs to tx keepalive, 
    uint16_t currentRXBufferSpace = vp->getBufSpace();
    if(currentRXBufferSpace > vp->lastRXBufferSpaceTransmitted || vp->lastTxTime + OSAP_TXKEEPALIVEINTERVAL < now){
      // has open space not reported, or needs to ping for port keepalive 
      if(vp->cts()){
        write77(_res, vp);
        vp->sendPacket(_res, 3);
        vp->decrimentRecipBufSpace();
      } 
    }
  } // end loop over ports (keepalive)
}