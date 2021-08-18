#ifndef __CPU_H__
#define __CPU_H__
#pragma once


#include "common.h"
#include "cpu.h"
#include "types.h"


/* 
  Errors
*/
#define ERR_INVALID_INSTRUCTION   0xdeadc0de



#define REG_B       0
#define REG_C       1
#define REG_D       2
#define REG_E       3
#define REG_H       4
#define REG_L       5
#define REG_M       6
#define REG_A       7

#define REGPAIR_BC  0x00
#define REGPAIR_DE  0x01
#define REGPAIR_HL  0x02
#define REGPAIR_PSW 0x03
#define REG_SP      0x04
#define REG_PC      0x05


/*
  Flag bits.

  NOTE: Bit 1 is unused and *always* 1. Bits 3 and 5 are unused
  and are *always* 0.
 */
#define FLG_CARRY    0x01
#define FLG_PARITY   0x04
#define FLG_AUXCRY   0x10
#define FLG_ZERO     0x40
#define FLG_SIGN     0x80


struct registers
{
  /* 
     Registers can be used as pairs in certain instructions. The
     possible pairs are:
     
        B & C
        D & E
        H & L
        
     Storing them sequentially in memory allows reading/writing
     these pairs with a single operation by casting to a word_t.
  */

  reg8_t B;
  reg8_t C;
  
  reg8_t D;
  reg8_t E;

  reg8_t H;
  reg8_t L;


  /* Accumulator */
  reg8_t A;
  /* Flags */
  reg8_t F;      

  /* Stack pointer */
  reg16_t SP;
  /* Program counter */
  reg16_t PC;
};


struct execute_params
{
  u8         instructionType;

  union
  {
    u8      regs[2];
    u16     regPair;
  };
};


#define CYCLE_COUNT_SHORT 0
#define CYCLE_COUNT_LONG  1

struct instruction
{
  /* Number of bytes used by the instruction including opcodes. Should
     be 1-3 */
  u8    byteCount;

  /* 
     Conditional call/return cycle counts differ based on whether or
     not action is taken. Index 0 stores when action is *NOT* taken;
     Index 1 stores when action *IS* taken.
  */
  u8    cycleCount[2];

  /* Used to decode the correct opcode (which register/pair on which
     to operate, etc) */
  struct execute_params executeParams;
  
};


void
CPU_Init(byte_t* memoryBlock);

void
CPU_DoInstructionCycle();

void
CPU_Fetch(byte_t* opcode);

void 
CPU_Decode(byte_t opcode);

void
CPU_AdvancePC();

void
CPU_Execute();

reg16_t
CPU_GetProgramCounter();

reg16_t
CPU_GetStackPointer();

byte_t
CPU_GetRegValue(u8 reg);

word_t
CPU_GetRegPairValue(u8 regPair);

bit_t
CPU_GetFlag(u8 flagBit);


#endif    /* __CPU_H__ */
