#ifndef __COMMON_H__
#define __COMMON_H__
#pragma once


#include "common.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h>




void
ErrorFatal(u32 errorCode);

u32
PowerOfTwo(u32 exp);

/*
  Return 1 if the number of 1 bits in the byte are even. Return 0 if
  the number of 1 bits in the byte are odd.
 */
bit_t
CheckBitParity(byte_t byte);

bool
IsSigned(byte_t byte);

bit_t
GetBit(byte_t data, u8 bitPosition);

void
SetBit(byte_t* data, u8 bitPosition, bit_t value);

void
SwapBytes(byte_t* a, byte_t* b);


#endif    /* __COMMON_H__ */
