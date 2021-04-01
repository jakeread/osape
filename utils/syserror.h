#ifndef SYSERROR_H_
#define SYSERROR_H_

#include <arduino.h>
#include "../../drivers/indicators.h"
#include "../osap/ts.h"
#include "cobs.h"

void sysError(String msg);
void logPacket(uint8_t* pck, uint16_t len);
//void sysError(uint8_t* bytes, uint16_t len);

#endif
