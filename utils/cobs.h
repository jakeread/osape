/*
utils/cobs.h

consistent overhead byte stuffing implementation

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo projects.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef UTIL_COBS_H_
#define UTIL_COBS_H_

#include <arduino.h>

size_t cobsEncode(uint8_t *src, size_t len, uint8_t *dest);

size_t cobsDecode(uint8_t *src, size_t len, uint8_t *dest);

#endif
