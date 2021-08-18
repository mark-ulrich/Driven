#ifndef __MEMORY_H__
#define __MEMORY_H__
#pragma once


#include "common.h"
#include "types.h"



byte_t*
Mem_Init(u32 size);

byte_t
Mem_ReadByte(word_t address);

word_t
Mem_ReadWord(word_t address);

void
Mem_WriteByte(word_t address, byte_t data);

void
Mem_WriteWord(word_t address, word_t data);

byte_t*
Mem_GetBytePointer(word_t address);


#endif    /* __MEMORY_H__ */
