#include "syserror.h"

uint8_t errBuf[1028];
uint8_t errEncoded[1028];

/*
boolean writeString(unsigned char* dest, uint16_t* dptr, String msg){
  uint16_t len = msg.length();
  dest[(*dptr) ++] = TS_STRING_KEY;
  writeLenBytes(dest, dptr, len);
  msg.getBytes(dest, len + 1);
  return true;
}

boolean writeLenBytes(unsigned char* dest, uint16_t* dptr, uint16_t len){
  dest[(*dptr) ++] = len;
  dest[(*dptr) ++] = (len >> 8) & 255;
  return true;
}
*/

// config-your-own-ll-escape-hatch
void sysError(String msg){
  // whatever you want,
  //ERRLIGHT_ON;
  uint32_t len = msg.length();
  errBuf[0] = PK_LLERR; // the ll-errmsg-key
  errBuf[1] = len & 255;
  errBuf[2] = (len >> 8) & 255;
  errBuf[3] = (len >> 16) & 255;
  errBuf[4] = (len >> 24) & 255;
  msg.getBytes(&errBuf[5], len + 1);
  size_t ecl = cobsEncode(errBuf, len + 5, errEncoded);
  if(Serial.availableForWrite() > (int64_t)ecl){
    Serial.write(errEncoded, ecl);
    Serial.flush();
  }
}
