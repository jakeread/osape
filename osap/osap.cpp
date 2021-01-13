/*
osap/osap.cpp

virtual network node

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "osap.h"

// -------------------------------------------------------- NODE CONSTRUCTION 

OSAP::OSAP(String nodeName){
  name = nodeName;
}

// try doing this a-la endpoint add, instead of this strange... thing, 
// use fn ptrs to static code, instead of vtables, yar? maybe faster / better way to run 
// a lot of interfaces 
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

Endpoint* OSAP::endpoint(boolean (*onNewData)(uint8_t* data, uint16_t len)){
  if(_numEndpoints >= OSAP_MAX_ENDPOINTS){
    return _endpoints[0]; // donot make new, loop... should find a way to catch errors
    // hopefully this will be an obvious bug though, when it appears 
  } else {
    _endpoints[_numEndpoints] = new Endpoint(onNewData);
    _numEndpoints ++;
    return _endpoints[_numEndpoints - 1];
  }
}

// -------------------------------------------------------- MAIN TRANSMIT CALL

boolean OSAP::send(uint8_t* txroute, uint16_t routelen, uint16_t segsize, uint8_t* data, uint16_t datalen){
  // can't do this, 
  if(routelen + datalen + 6 > segsize) return false;
  // 1st order of business, we pull the outgoing vport from the route, 
  uint16_t rptr = 1;
  uint16_t vpi = 0;
  ts_readUint16(&vpi, txroute, &rptr);
  // check oob ports
  if(vpi >= _numVPorts){
    return false; 
  }
  // switch dep. on msg / port type 
  VPort* vpo = _vPorts[vpi];
  switch(txroute[0]){
    case PK_PORTF_KEY: {
      // match vport type and route description 
      if(vpo->portTypeKey != EP_PORTTYPEKEY_DUPLEX) return false;
      if(!(vpo->cts(0))) return false;
      // copy route 
      memcpy(_tx, txroute, 3);
      _tx[3] = PK_PTR; 
      memcpy(&(_tx[4]), &(txroute[3]), routelen - 3);
      // destination station 
      uint16_t wptr = routelen + 1;
      _tx[wptr ++] = PK_DEST;
      ts_writeUint16(segsize, _tx, &wptr);
      ts_writeUint16(datalen, _tx, &wptr);
      // actual datagram 
      memcpy(&(_tx[wptr]), data, datalen);
      // dispatch it 
      vpo->send(_tx, wptr + datalen, 0); // 0 here is the 'rxaddress', meaningless on a duplex link
      return true;
      break;
    }
    case PK_BUSF_KEY: {
      // match vport type w/ route description 
      if(vpo->portTypeKey != EP_PORTTYPEKEY_BUSHEAD && vpo->portTypeKey != EP_PORTTYPEKEY_BUSDROP) return false;
      uint16_t rxaddr = 0;
      ts_readUint16(&rxaddr, txroute, &rptr);
      // can't transmit to anyone but bus head, from a drop 
      if(vpo->portTypeKey == EP_PORTTYPEKEY_BUSDROP && rxaddr != 0) return false;
      // can't transmit if not cts, big brain energy 
      if(!(vpo->cts(rxaddr))) return false;
      // can copy the route in 
      memcpy(_tx, txroute, 5);  // outgoing instruction 
      _tx[5] = PK_PTR;          // ptr to next instruction (for recipient to execute)
      memcpy(&(_tx[6]), &(txroute[5]), routelen - 5);  // remainder of route
      // copy the destination work:
      uint16_t wptr = routelen + 1;
      _tx[wptr ++] = PK_DEST;
      ts_writeUint16(segsize, _tx, &wptr);
      ts_writeUint16(datalen, _tx, &wptr);
      // and finally, the data 
      memcpy(&(_tx[wptr]), data, datalen);
      // aaaaand we can ship it 
      vpo->send(_tx, wptr + datalen, rxaddr);
      return true;
      break;
    }
    case PK_BUSB_KEY: {
      if(vpo->portTypeKey != EP_PORTTYPEKEY_BUSHEAD) return false;
      // eeeeh, not supporting these yet 
      return false;
      break;
    }
    default:
      return false;
  }
}

// -------------------------------------------------------- HANDLE ENDPOINT PACKET 

// pck[ptr] == DK_VMODULE, next bytes are 
void OSAP::handleVModulePacket(uint8_t* pck, uint16_t ptr, pckm_t* pckm){
  ptr ++; // advance past key, 
  uint16_t vmFrom, epFrom, vmTo, epTo;
  ts_readUint16(&vmFrom, pck, &ptr);
  ts_readUint16(&epFrom, pck, &ptr);
  ts_readUint16(&vmTo, pck, &ptr);
  ts_readUint16(&epTo, pck, &ptr);
  // ATM, yrs truly is ignoring the vm's, assuming all osaps are flat 
  if(epTo > _numEndpoints){
    // doth not exist, drop 
    pckm->vpa->clear(pckm->location);
  } else {
    Endpoint* ep = _endpoints[epTo];
    if(ep->dataNew){ // bb is already occupied / hasn't been handled 
      if(pckm->vpa->cts(pckm->txAddr)){
        // write & send the nack 
        uint8_t nack[9];
        uint16_t wptr = 0;
        // modules reversed: a nack from us, to them
        nack[wptr ++] = DK_VMODULE_NACK;
        ts_writeUint16(vmTo, nack, &wptr);
        ts_writeUint16(epTo, nack, &wptr);
        ts_writeUint16(vmFrom, nack, &wptr);
        ts_writeUint16(epFrom, nack, &wptr);
        // ship the yack 
        appReply(pck, pckm, nack, 9);
      } else {
        // the missed nack, will timeout instead at transmit side 
        pckm->vpa->clear(pckm->location);
      }
    } else {
      ep->fill(pck, ptr, pckm, vmFrom, epFrom);
      // try to single-shot handle it, 
      if(ep->consume()){
        ep->dataNew = false;
        if(pckm->vpa->cts(pckm->txAddr)){
          // write & send the yack, if we can tx 
          uint8_t yack[9];
          uint16_t wptr = 0;
          // modules reversed: a nack from us, to them
          yack[wptr ++] = DK_VMODULE_YACK;
          ts_writeUint16(vmTo, yack, &wptr);
          ts_writeUint16(epTo, yack, &wptr);
          ts_writeUint16(vmFrom, yack, &wptr);
          ts_writeUint16(epFrom, yack, &wptr);
          // ship the yack 
          appReply(pck, pckm, yack, 9);
        } else {
          // not clear to cts, but have consumed, 
          // this is the missed return, 
          pckm->vpa->clear(pckm->location);
        }
      } else {
        // no-op, osap will retry later 
      }
    }
  }
}

// -------------------------------------------------------- REPLIES

// packet to read from, response to write into is global _res
// apparent position of pck[ptr] == DK_(?) (i.e. 1st byte of payload)
// packet meta (len, vport of arrival, tx/rx addresses, etc)
// this reverses the route, out of the packet, into the _res, 
// and writes the checksum, seglength, but not the data 
boolean OSAP::formatResponseHeader(uint8_t *pck, uint16_t ptr, pckm_t* pckm, uint16_t reslen){
  // when called, pck[ptr] == 1st byte of datagram (probably a destination key) 
  //                                          [pck[ptr]]
  // [route][ptr][dest][segsize:2][checksum:2][datagram]
  // so if we do, 
  uint16_t eor = ptr - 5; // end of the route is the dest key, here 
  uint16_t wptr = eor;    // first, re-write the 5 dest bytes  
  _res[wptr ++] = PK_DEST;                      // end of route is here, destination key 
  ts_writeUint16(pckm->segSize, _res, &wptr);   // segsize follows destination mark 
  ts_writeUint16(reslen, _res, &wptr);          // checksum follows segsize 
  // now write the first / outgoing instruction, and the pck_ptr key that follows 
  wptr = 0;
  switch(pckm->vpa->portTypeKey){
    case EP_PORTTYPEKEY_DUPLEX: // arrival was duplex, 
      _res[wptr ++] = PK_PORTF_KEY;
      ts_writeUint16(pckm->vpa->indice, _res, &wptr);  // it's leaving on the same vport was rx'd on, 
      break;
    case EP_PORTTYPEKEY_BUSHEAD:
    case EP_PORTTYPEKEY_BUSDROP:
      _res[wptr ++] = PK_BUSF_KEY;
      ts_writeUint16(pckm->vpa->indice, _res, &wptr);  // same: exit from whence you came 
      ts_writeUint16(pckm->txAddr, _res, &wptr);       // return to the bus address where you originated
      break;
    default: // something is broken, 
      pckm->vpa->clear(pckm->location);
      return false;
  }
  _res[wptr ++] = PK_PTR;  // after outgoing instruction, the instruction ptr to next instruction, 
  // now, read old instructions from the head of the packet, write them in at the tail 
  uint16_t backstop = wptr;   // don't write any further back than this,
  wptr = eor;                 // write starting at end of packet (backing up each step)
  uint16_t rptr = 0;          // read from the old instructions starting at the head of the packet
  // do max. 16 packet steps 
  for(uint8_t h = 0; h < 16; h ++){
    if(wptr <= backstop) break;
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
  } // end walks,
  if(wptr != backstop){ // ptrs should match up when done, 
    sysError("bad response header write");
    return false;
  } else {
    return true;
  }
}

// write and send ahn reply out, 
// WARNING: application needs to check if the vport to reply on (in pckm) is clear 
void OSAP::appReply(uint8_t *pck, pckm_t* pckm, uint8_t *reply, uint16_t repLen){
  // need to find the ptr again, 
  uint16_t ptr = 0;
  for(uint8_t i = 0; i < 16; i ++){
    switch(pck[ptr]){
      case PK_PTR:
        ptr += 6; // ptr to 1st byte of pck datagram, where formatResponseHeader operates 
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
      default:
        // no dice in the forward walk, clear it 
        sysError("bad walk for ptr: key " + String(pck[ptr]) + " at: " + String(ptr));
        pckm->vpa->clear(pckm->location);
        return; // this shouldn't happen, but let's not try to transmit it on 
    } // end switch
  } // end loop for ptr walk,
  endWalk: ; // break the walk-loop to here
  // copy the reply in to our _res global. 
  uint16_t wptr = ptr;
  for(uint16_t i = 0; i < repLen; i ++){
    _res[wptr ++] = reply[i];
  }
  // reverse the route, ship it 
  if(formatResponseHeader(pck, ptr, pckm, repLen)){
    pckm->vpa->send(_res, wptr, pckm->txAddr);
  } else {
    sysError("bad response format");
    pckm->vpa->clear(pckm->location); // broken packet rx'd, so, bail 
  }
}

// -------------------------------------------------------- MAIN HANDLE LOOP 

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
          pckm->segSize = segsize;
          pckm->checksum = checksum;
          switch(pck[ptr]){
            case DK_APP:
              handleAppPacket(pck, ptr, pckm);
              break;
            case DK_VMODULE:
              handleVModulePacket(pck, ptr, pckm);
              break;
            case DK_PINGREQ:
              handlePingRequest(pck, ptr, pckm);
              break;
            case DK_EPREQ:
              handleEntryPointRequest(pck, ptr, pckm);
              break;
            case DK_RREQ:
              handleReadRequest(pck, ptr, pckm);
              break;
              break;
            case DK_WREQ: // no writing yet,
            case DK_PINGRES: // not issuing pings from embedded, shouldn't have deal w/ their responses
            case DK_RRES: // not issuing requests from embedded, same
            case DK_WRES: // not issuing write requests from embedded, again
              sysError("WREQ or RES received in embedded, popping");
              pckm->vpa->clear(pckm->location);
              break;
            default:
              sysError("non-recognized destination key, popping");
              pckm->vpa->clear(pckm->location);
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
  // do loop over endpoints, 
  for(uint8_t ep = 0; ep < _numEndpoints; ep ++){
    if(_endpoints[ep]->dataNew){
      Endpoint* ept = _endpoints[ep];
      if(ept->consume()){
        ept->dataNew = false;
        if(ept->pckmStore.vpa->cts(ept->pckmStore.txAddr)){
        // eats it OK, so we can clear 
        uint8_t yack[9];
        uint16_t wptr = 0;
        yack[wptr ++] = DK_VMODULE_YACK;
        // reversed, 
        ts_writeUint16(0, yack, &wptr); // recall, no submodules atm
        ts_writeUint16(ept->indice, yack, &wptr);
        ts_writeUint16(ept->rxFromVM, yack, &wptr);
        ts_writeUint16(ept->rxFromEP, yack, &wptr);
        // ship the yack 
        appReply(ept->pckStore, &(ept->pckmStore), yack, 9);
        ept->dataNew = false;
        } else {
          // the missed ack, 
        }
      } else {
        // no-op, will continue to retry 
      }
    }
  }
}