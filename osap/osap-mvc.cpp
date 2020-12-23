/*
osap/osap-mvc.cpp

virtual network node, handlers for serialization of system info 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "osap.h"

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
void OSAP::readRequestVPort(uint8_t *pck, uint16_t ptr, pckm_t* pckm, uint16_t rptr, uint16_t *wptr, VPort* vp){
  // could be terminal, could read into endpoints (input / output) of the vport,
  for(uint8_t i = 0; i < 16; i ++){
    if(rptr >= pckm->len){
      return;
    }
    if(*wptr > pckm->segSize){
      sysError("QUERYDOWN wptr: " + String(*wptr) + " segsize: " + String(pckm->segSize));
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
        _res[(*wptr) ++] = vp->status();
        rptr ++;
        break;
      default:
        writeEmpty(wptr);
        return;
    }
  }
}

void OSAP::handleReadRequest(uint8_t *pck, uint16_t ptr, pckm_t* pckm){
  if(!(pckm->vpa->cts(pckm->txAddr))){ return; } // await ready-to-respond, 
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
    if(rptr >= pckm->len){
      goto endwalk;
    }
    if(wptr > pckm->segSize){
      sysError("QUERYDOWN wptr: " + String(wptr) + " segsize: " + String(pckm->segSize));
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
          readRequestVPort(pck, ptr, pckm, rptr, &wptr, _vPorts[indice]);
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
  if(formatResponseHeader(pck, ptr, pckm, wptr - ptr)){
    pckm->vpa->clear(pckm->location);
    pckm->vpa->send(_res, wptr, pckm->txAddr);
  } else {
    sysError("bad read response format");
    pckm->vpa->clear(pckm->location);
  }
}

void OSAP::handleEntryPointRequest(uint8_t* pck, uint16_t ptr, pckm_t* pckm){
  if(!(pckm->vpa->cts(pckm->txAddr))){ return; }
  uint16_t wptr = ptr;
  _res[wptr ++] = DK_EPRES;
  _res[wptr ++] = pck[ptr + 1];
  _res[wptr ++] = pck[ptr + 2];
  ts_writeUint16(pckm->vpa->indice, _res, &wptr);
  if(formatResponseHeader(pck, ptr, pckm, 5)){
    pckm->vpa->clear(pckm->location);
    pckm->vpa->send(_res, wptr, pckm->txAddr);
  } else {
    sysError("at ep req, bad response format");
    pckm->vpa->clear(pckm->location);
  }
}

// pck[ptr] == DK_PINGREQ
void OSAP::handlePingRequest(uint8_t *pck, uint16_t ptr, pckm_t* pckm){
  // check if we'll be able to respond. for p2p cases, the bus rxAddr is ignored, so... 
  if(!(pckm->vpa->cts(pckm->txAddr))){ return; }
  // the reversed header will be *the same length* as the received header,
  // which was from 0-ptr! - this is great news, we can say:
  uint16_t wptr = ptr;          // start writing here, leaves room for the header,
  _res[wptr ++] = DK_PINGRES;   // write in whatever the response is, here just the ping-res key and id
  _res[wptr ++] = pck[ptr + 1]; // copy in the ping id, 
  _res[wptr ++] = pck[ptr + 2];
  // this'll be the 'std' response formatting codes,
  // formatResponseHeader doesn't need the _res, that's baked in, and it writes 0-ptr,
  // since we wrote into _res following ptr, (header lengths identical), this is safe,
  if(formatResponseHeader(pck, ptr, pckm, 3)){ // write the header: this goes _resp[3] -> _resp[ptr]
    pckm->vpa->clear(pckm->location);
    pckm->vpa->send(_res, wptr, pckm->txAddr);
  } else {
    pckm->vpa->clear(pckm->location);
  }
}