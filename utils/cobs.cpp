/*
utils/cobs.cpp

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "cobs.h"
// str8 crib from
// https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing

#define StartBlock()	(code_ptr = dst++, code = 1)
#define FinishBlock()	(*code_ptr = code)

size_t cobsEncode(uint8_t *ptr, size_t length, uint8_t *dst){
  const uint8_t *start = dst, *end = ptr + length;
  uint8_t code, *code_ptr; /* Where to insert the leading count */

  StartBlock();
  while (ptr < end) {
  	if (code != 0xFF) {
  		uint8_t c = *ptr++;
  		if (c != 0) {
  			*dst++ = c;
  			code++;
  			continue;
  		}
  	}
  	FinishBlock();
  	StartBlock();
  }
  FinishBlock();
  // write the actual zero,
  *dst++ = 0;
  return dst - start;
}

size_t cobsDecode(uint8_t *ptr, size_t length, uint8_t *dst)
{
	const uint8_t *start = dst, *end = ptr + length;
	uint8_t code = 0xFF, copy = 0;

	for (; ptr < end; copy--) {
		if (copy != 0) {
			*dst++ = *ptr++;
		} else {
			if (code != 0xFF)
				*dst++ = 0;
			copy = code = *ptr++;
			if (code == 0)
				break; /* Source length too long */
		}
	}
	return dst - start;
}
