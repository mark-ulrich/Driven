#include "common.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>

void ErrorFatal(u32 errorCode) {
  fprintf(stderr, "FATAL: %u\n", errorCode);
  abort();
}

u32 PowerOfTwo(u32 exp) {
  u32 i;
  u32 result = 1;
  for (i = 0; i < exp; ++i) {
    result *= 2;
  }
  return result;
}

/*
  Return 1 if the number of 1 bits in the byte are even. Return 0 if
  the number of 1 bits in the byte are odd.
 */
bit_t CheckBitParity(byte_t byte) {
  int i;
  int oneBitCount;

  oneBitCount = 0;
  for (i = 0; i < 8; ++i) {
    oneBitCount += (byte >> i) & 1;
  }

  return !(oneBitCount & 1);
  /* return (oneBitCount % 2) != 0; */
}

bool IsSigned(byte_t byte) {
  if (byte & 0x80)
    return true;
  return false;
}

bit_t GetBit(byte_t data, u8 bitPosition) {
  return (data & (1 << bitPosition)) >> bitPosition;
}

void SetBit(byte_t *data, u8 bitPosition, bit_t value) {

#ifdef _DEBUG
  Log_Debug("SetBit: bitPosition=%u value=%u", bitPosition, value);
#endif

  if (value == 0)
    *data &= ~(1 << bitPosition);
  else
    *data |= (1 << bitPosition);
}

void SwapBytes(byte_t *a, byte_t *b) {
  byte_t tmp;
  tmp = *a;
  *a = *b;
  *b = tmp;
}
