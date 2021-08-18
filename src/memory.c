#include "common.h"
#include "log.h"
#include "memory.h"



internal byte_t* memory;
internal u32     memSize;

byte_t*
Mem_Init(u32 size)
{
  memory  = (byte_t*)malloc(size);
  memSize = size;
  return memory;
}

byte_t
Mem_ReadByte(word_t address)
{
  if (address > memSize - 1)
  {
    // TODO: Out of range
    return 0;
  }

  return memory[address];
}

word_t
Mem_ReadWord(word_t address)
{
  if (address + 1 > memSize - 1)
  {
    // TODO: Out of range
    return 0;
  }

  return ((memory[address+1] << 8) | memory[address]);
}

void
Mem_WriteByte(word_t address, byte_t data)
{
  if (address > memSize - 1)
  {
    // TODO: Out of range
    abort();
  }

  memory[address] = data;
}

void
Mem_WriteWord(word_t address, word_t data)
{

#ifdef _DEBUG
  Log_Debug("Mem_WriteWord: address=0x%04x data=0x%04x", address, data);
#endif
  
  if (address > memSize - 1)
  {
    // TODO: Out of range
    abort();
  }

  memory[address]   = (byte_t)data;
  memory[address+1] = (byte_t)(data >> 8);
}

byte_t*
Mem_GetBytePointer(word_t address)
{
  if (address > memSize - 1)
  {
    // TODO: Out of range
    return 0;
  }

  return &memory[address];
}
