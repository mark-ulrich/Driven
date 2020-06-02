/*
  Name: Driven
  Puprose: An Intel 8080 CPU emulator
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/*
  TODO: Move types to a common header
*/
typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u8  bit_t;
typedef u8  byte_t;
typedef u16 word_t; 

typedef byte_t reg8_t;
typedef word_t reg16_t; 


#define internal     static



#define ERR_INVALID_INSTRUCTION   0xdeadc0de


void
ErrorFatal(u32 errorCode)
{
  fprintf(stderr, "FATAL: %u\n", errorCode);
  abort();
}


byte_t* memory;



/*
  Flag bits.

  NOTE: Bit 1 is unused and *always* 1. Bits 3 and 5 are unused
  and are *always* 0.
 */
#define FLG_CARRY    0
#define FLG_PARITY   2
#define FLG_AUXCRY   4
#define FLG_ZERO     6
#define FLG_SIGN     7

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
struct registers regs;



#define CYCLE_COUNT_SHORT 0
#define CYCLE_COUNT_LONG  1

struct execute_params
{
  u8         instructionType;

  union
  {
    u8      regs[2];
    u16     regPair;
  };

  /*
  union 
  { 
    word_t   immediateData; 
    word_t   address; 
  }; 
  */
};

#define EXECUTE_PROC 
/* typedef (*(void)()) ExecuteProc; */
struct instruction
{
  /* Number of bytes used by the instruction including opcodes. Should be 1-3 */
  u8    byteCount;

  /* 
     Conditional call/return cycle counts differ based on whether or
     not action is taken. Index 0 stores when action is *NOT* taken;
     Index 1 stores when action *IS* taken.
  */
  u8    cycleCount[2];

  //  void (*executeProc)(/*struct execute_proc_params*/);
  struct execute_params executeParams;
  
};


/*
int
RaiseToPower(int base, int exp)
{
  int i;
  int result = 1;
  for (i = 0;
       i < exp;
       ++i)
  {
    result *= base;
  }
  return result;
}
*/

u32
PowerOfTwo(u32 exp)
{
  u32 i;
  u32 result = 1;
  for (i = 0;
       i < exp;
       ++i)
  {
    result *= 2;
  }
  return result;
}



/*
  Return 1 if the number of 1 bits in the byte are even. Return 0 if
  the number of 1 bits in the byte are odd.
 */
bit_t
CheckByteParity(byte_t byte)
{
  int i;
  int oneCount;

  oneCount = 0;
  for (i = 0;
       i < 8;
       ++i)
  {
    oneCount += (byte >> i) & 1;
  }

  return !(oneCount % 2);
}

bool
IsSigned(byte_t byte)
{
  if (byte >> 7) return true;
  return false;
}




byte_t
Mem_ReadByte(word_t address);

void
Mem_WriteByte(byte_t address, byte_t data);

byte_t*
Mem_GetBytePointer(word_t address);




#define REG_B    0
#define REG_C    1
#define REG_D    2
#define REG_E    3
#define REG_H    4
#define REG_L    5
#define REG_A    6
#define REG_M    10

#define REGPAIR_BC  0
#define REGPAIR_DE  1
#define REGPAIR_HL  2
#define REGPAIR_PSW 3


internal reg8_t*
CPU_GetRegPointer(u8 reg)
{
  switch (reg)
  {
  case REG_B:
    {
      return &regs.B;
    }

  case REG_C:
    {
      return &regs.C;
    }

  case REG_D:
    {
      return &regs.D;
    }

  case REG_E:
    {
      return &regs.E;
    }

  case REG_H:
    {
      return &regs.H;
    }

  case REG_L:
    {
      return &regs.L;
    }

  case REG_A:
    {
      return &regs.A;
    }

  default:
    return 0;
  }
}

byte_t
CPU_GetRegValue(u8 reg)
{
  return *CPU_GetRegPointer(reg);
}

/*
  Get the value of a bit in the Flags register
*/
internal bit_t
CPU_GetFlag(u8 flagBit)
{
  return ((regs.F >> flagBit) & 1);
}

/*
  Set the value of a bit in the Flags register
*/
internal void
CPU_SetFlag(u8 flagBit, bit_t state)
{
  if (state == 0)
  {
    regs.F &= ~(1 << flagBit);
  }
  else
  {
    regs.F |= (1 << flagBit);
  }
}

/*
  Toggle a bit in the flags register
*/
internal void
CPU_ToggleFlag(u8 flagBit)
{
  regs.F ^= (1 << flagBit);
}


/*
  Attempt to implement a simple ALU Adder.

  TODO: This is super ugly. Do some research and fix this!
 */
void
ALU_Adder(byte_t* byte, u8 addend)
{
  u8 bitIndex;
  byte_t sum = 0;
  bit_t carry = 0;
  for (bitIndex = 0;
       bitIndex < 8;
       ++bitIndex)
  {
    bit_t byteBit   = (*byte  >> bitIndex) & 1;
    bit_t addendBit = (addend >> bitIndex) & 1;

    bit_t tempBit   = (byteBit ^ addendBit);
    tempBit ^= carry;

    carry = ((byteBit + addendBit + carry) > 1);

    sum += tempBit << bitIndex;

    if (bitIndex == 3)
    {
      CPU_SetFlag(FLG_AUXCRY, carry);
    }
  }
  *byte = sum;
  CPU_SetFlag(FLG_CARRY, carry);
}


/*
  Constants for each of the 8080's instruction types
*/
enum {
      INSTR_ACI,
      INSTR_ADC,
      INSTR_ADD,
      INSTR_ADI,
      INSTR_ANA,
      INSTR_ANI,
      INSTR_CALL,
      INSTR_CC,
      INSTR_CM,
      INSTR_CMA,
      INSTR_CMC,
      INSTR_CMP,
      INSTR_CNC,
      INSTR_CNZ,
      INSTR_CP,
      INSTR_CPE,
      INSTR_CPI,
      INSTR_CPO,
      INSTR_CZ,
      INSTR_DAA,
      INSTR_DAD,
      INSTR_DCR,
      INSTR_DCX,
      INSTR_EI,
      INSTR_HLT,
      INSTR_IN,
      INSTR_INR,
      INSTR_INX,
      INSTR_JC,
      INSTR_JM,
      INSTR_JMP,
      INSTR_JNC,
      INSTR_JNZ,
      INSTR_JP,
      INSTR_JPE,
      INSTR_JPO,
      INSTR_JZ,
      INSTR_LDA,
      INSTR_LDAX,
      INSTR_LHLD,
      INSTR_LXI,
      INSTR_MOV,
      INSTR_MVI,
      INSTR_NOP,
      INSTR_ORA,
      INSTR_OUT,
      INSTR_PCHL,
      INSTR_POP,
      INSTR_PUSH,
      INSTR_RAL,
      INSTR_RAR,
      INSTR_RC,
      INSTR_RET,
      INSTR_RLC,
      INSTR_RM,
      INSTR_RNC,
      INSTR_RP,
      INSTR_RPE,
      INSTR_RPO,
      INSTR_RRC,
      INSTR_RST,
      INSTR_RZ,
      INSTR_SBB,
      INSTR_SBI,
      INSTR_SHLD,
      INSTR_SPHL,
      INSTR_STA,
      INSTR_STAX,
      INSTR_STC,
      INSTR_SUB,
      INSTR_SUI,
      INSTR_XCHG,
      INSTR_XRA,
      INSTR_XRI,
      INSTR_XTHL
};



internal void
Execute_ACI()
{
}

/*
  ADC - Add Register or Memory to Accumulator With Carry

  The specified byte plus the content of the Carry bit is added to the
  contents of the accumulator.
  
  Flags affected: C, S, Z, P, AC
*/
internal void
Execute_ADC(byte_t addend)
{
  ALU_Adder(&regs.A, addend);
  if (CPU_GetFlag(FLG_CARRY))
    ALU_Adder(&regs.A, 1);
  
  CPU_SetFlag(FLG_ZERO,   (regs.A == 0));
  CPU_SetFlag(FLG_SIGN,   IsSigned(regs.A));
  CPU_SetFlag(FLG_PARITY, CheckByteParity(regs.A));
}

/*
  ADD - Add Register or Memory to Accumulator
  
  The specified byte is added to the contents of the accumulator using
  twos's complement arithmetic.
  
  Flags affected: C, S, Z, P, AC
*/
internal void
Execute_ADD(byte_t addend)
{
  /* TODO: Make sure the ALU_Adder is using twos-complement */
  ALU_Adder(&regs.A, addend);

  CPU_SetFlag(FLG_ZERO,   (regs.A == 0));
  CPU_SetFlag(FLG_SIGN,   IsSigned(regs.A));
  CPU_SetFlag(FLG_PARITY, CheckByteParity(regs.A));
}

internal void
Execute_ADI()
{
}

internal void
Execute_ANA()
{
}

internal void
Execute_ANI()
{
}

internal void
Execute_CALL()
{
}

internal void
Execute_CC()
{
}

internal void
Execute_CM()
{
}

/*
  CMA - Complement Accumulator

  Each it of the contents of the accumulator is complemented.

  Flags affected: None
 */
internal void
Execute_CMA()
{
  regs.A = ~regs.A;
}

/*
  CMC - Complement Carry

  If the Carry bit is 0, it is set to 1.
  If the Carry bit is 1, it is set to 0.

  Flags affected: C
 */
internal void
Execute_CMC(void)
{
  CPU_ToggleFlag(FLG_CARRY);
}

internal void
Execute_CMP()
{
}

internal void
Execute_CNC()
{
}

internal void
Execute_CNZ()
{
}

internal void
Execute_CP()
{
}

internal void
Execute_CPE()
{
}

internal void
Execute_CPI()
{
}

internal void
Execute_CPO()
{
}

internal void
Execute_CZ()
{
}

/*
  DAA - Decimal Adjust Accumulator
  
  See the Intel 8080 Reference

  Flags affected: Z, S, P, C, AC
*/
internal void
Execute_DAA()
{
  if ((regs.A & 0x0f) > 9 ||
      CPU_GetFlag(FLG_AUXCRY))
  {
    ALU_Adder(&regs.A, 6);
  }
  if (((regs.A & 0xf0) >> 4) > 9 ||
      CPU_GetFlag(FLG_CARRY))
  {
    ALU_Adder(&regs.A, (6 << 4));
  }

  CPU_SetFlag(FLG_ZERO,   (regs.A == 0));
  CPU_SetFlag(FLG_SIGN,   IsSigned(regs.A));
  CPU_SetFlag(FLG_PARITY, CheckByteParity(regs.A));
}

internal void
Execute_DAD()
{
}

/*
  DCR - Decrement Register or Memory
  
  The specified register or memory bit is decremented by one.

  Flags affected: Z, S, P, AC
*/
internal void
Execute_DCR(byte_t* byte)
{
  --*byte;
}

internal void
Execute_DCX()
{
}

internal void
Execute_EI()
{
}

internal void
Execute_HLT()
{
}

internal void
Execute_IN()
{
}

/*
  INR - Increment Register or Memory
  
  The specified register or memory bit is incremented by one.

  Flags affected: Z, S, P, AC
*/
internal void
Execute_INR(byte_t* byte)
{
  ++*byte;
}

internal void
Execute_INX()
{
}

internal void
Execute_JC()
{
}

internal void
Execute_JM()
{
}

internal void
Execute_JMP()
{
}

internal void
Execute_JNC()
{
}

internal void
Execute_JNZ()
{
}

internal void
Execute_JP()
{
}

internal void
Execute_JPE()
{
}

internal void
Execute_JPO()
{
}

internal void
Execute_JZ()
{
}

internal void
Execute_LDA()
{
}

/*
  LDAX - Load Accumulator
  
  The contents of the memory location addressed by the specified
  registers (B and C or D and E) replace the contents of the
  accumulator.
  
  Flags affected: None
*/
internal void
Execute_LDAX(word_t address)
{
  regs.A = Mem_ReadByte(address);
}

internal void
Execute_LHLD()
{
}

internal void
Execute_LXI()
{
}

/*
  MOV

  One byte of data is moved from the src register to the dst register.
  
  Flags affected: None
*/
internal void
Execute_MOV(byte_t* dst, byte_t* src)
{
  *dst = *src;
}

/*
  MVI

  Flags affected: ....
*/
internal void
Execute_MVI(byte_t* dst, byte_t data)
{
  *dst = data;
}

internal void
Execute_NOP()
{
  /* NOPE */
}

internal void
Execute_ORA()
{
}

internal void
Execute_OUT()
{
}

internal void
Execute_PCHL()
{
}

internal void
Execute_POP()
{
}

internal void
Execute_PUSH()
{
}

internal void
Execute_RAL()
{
}

internal void
Execute_RAR()
{
}

internal void
Execute_RC()
{
}

internal void
Execute_RET()
{
}

internal void
Execute_RLC()
{
}

internal void
Execute_RM()
{
}

internal void
Execute_RNC()
{
}

internal void
Execute_RP()
{
}

internal void
Execute_RPE()
{
}

internal void
Execute_RPO()
{
}

internal void
Execute_RRC()
{
}

internal void
Execute_RST()
{
}

internal void
Execute_RZ()
{
}

internal void
Execute_SBB()
{
}

internal void
Execute_SBI()
{
}

internal void
Execute_SHLD()
{
}

internal void
Execute_SPHL()
{
}

internal void
Execute_STA()
{
}

/*
  STAX - Store Accumulator
  
  The contents of the accumulator are stored in the memory addressed
  by the given registers (B and C or D and E).
  
  Flags affected: None
*/
internal void
Execute_STAX(byte_t address)
{
  Mem_WriteByte(address, regs.A);
}

/*
  STC - Set Carry
  
  The cary bit is set to one.

  Flags affected: C
*/
internal void
Execute_STC()
{
  CPU_SetFlag(FLG_CARRY, 1);
}

internal void
Execute_SUB()
{
}

internal void
Execute_SUI()
{
}

internal void
Execute_XCHG()
{
}

internal void
Execute_XRA()
{
}

internal void
Execute_XRI()
{
}

internal void
Execute_XTHL()
{
}







/*
  The instruction set array is built so that the index equates to the
  specific opcode.

  This array was built from a reference at:
    https://pastraiser.com/cpu/i8080/i8080_opcodes.html
*/
struct instruction instruction_set[256] =
  {
   /* 0x00: NOP */
   { 1, { 4, 4 }, { INSTR_NOP } },
   /* 0x01: LXI B,d16 */
   { 3, { 10, 10}, { INSTR_LXI } },
   /* 0x02: STAX B */
   { 1, { 7, 7}, { INSTR_STAX } },
   /* 0x03: INX B */
   { 1, { 5, 5 }, { INSTR_INX } },
   /* 0x04: INR B */
   { 1, { 5, 5 }, { INSTR_INR } },
   /* 0x05: DCR B */
   { 1, { 5, 5 }, { INSTR_DCR } },
   /* 0x06: MVI B,d8 */
   { 2, { 7, 7 }, { INSTR_MVI } },
   /* 0x07: RLC */
   { 1, { 4, 4 }, { INSTR_RLC } },
   /* 0x08: *NOP */
   { 1, { 4, 4 }, { INSTR_RLC } },
   /* 0x09: DAD B */
   { 1, { 10, 10 }, { INSTR_RLC } },
   /* 0x0A: LDAX B */
   { 1, { 7, 7 }, { INSTR_LDAX } },
   /* 0x0B: DCX B */
   { 1, { 5, 5 }, { INSTR_DCX } },
   /* 0x0C: INR C */
   { 1, { 5, 5 }, { INSTR_INR } },
   /* 0x0D: DCR C */
   { 1, { 5, 5 }, { INSTR_DCR } },
   /* 0x0E: MVI C,d8 */
   { 2, { 7, 7 }, { INSTR_MVI } },
   /* 0x0F: RRC */
   { 1, { 4, 4 }, { INSTR_RRC } },
   /* 0x10: *NOP */
   { 1, { 4, 4 }, { INSTR_NOP } },
   /* 0x11: LXI D,d16 */
   { 3, { 10, 10 }, { INSTR_LXI } },
   /* 0x12: STAX D */
   { 1, { 7, 7 }, { INSTR_STAX } },
   /* 0x13: INX D */
   { 1, { 5, 5 }, { INSTR_INX } },
   /* 0x14: INR D */
   { 1, { 5, 5 }, { INSTR_INR } },
   /* 0x15: DCR D */
   { 1, { 5, 5 }, { INSTR_DCR } },
   /* 0x16: MVI D,d8 */
   { 2, { 7, 7 }, { INSTR_MVI } },
   /* 0x17: RAL */
   { 1, { 4, 4 }, { INSTR_RAL } },
   /* 0x18: *NOP */
   { 1, { 4, 4 }, { INSTR_NOP } },
   /* 0x19: DAD D */
   { 1, { 10, 10 }, { INSTR_DAD } },
   /* 0x1A: LDAX D */
   { 1, { 7, 7 }, { INSTR_LDAX } },
   /* 0x1B: DCX D */
   { 1, { 5, 5 }, { INSTR_DCX } },
   /* 0x1C: INR E */
   { 1, { 5, 5 }, { INSTR_INR } },
   /* 0x1D: DCR E */
   { 1, { 5, 5 }, { INSTR_DCR } },
   /* 0x1E: MVI E,d8 */
   { 2, { 7, 7 }, { INSTR_MVI } },
   /* 0x1F: RAR */
   { 1, { 4, 4 }, { INSTR_RAR } },
   /* 0x20: *NOP */
   { 1, { 4, 4 }, { INSTR_NOP } },
   /* 0x21: LXI H,d16 */
   { 3, { 10, 10 }, { INSTR_LXI } },
   /* 0x22: SHLD a16 */
   { 3, { 16, 16 }, { INSTR_SHLD } },
   /* 0x23: INX H */
   { 1, { 5, 5 }, { INSTR_INX } },
   /* 0x24: INR H */
   { 1, { 5, 5 }, { INSTR_INR } },
   /* 0x25: DCR H */
   { 1, { 5, 5 }, { INSTR_DCR } },
   /* 0x26: MVI H,d8 */
   { 2, { 7, 7 }, { INSTR_MVI } },
   /* 0x27: DAA */
   { 1, { 4, 4 }, { INSTR_DAA } },
   /* 0x28: *NOP */
   { 1, { 4, 4 }, { INSTR_NOP } },
   /* 0x29: DAD H */
   { 1, { 10, 10 }, { INSTR_DAD } },
   /* 0x2A: LHLD a16 */
   { 3, { 16, 16 }, { INSTR_LHLD } },
   /* 0x2B: DCX H */
   { 1, { 5, 5 }, { INSTR_DCX } },
   /* 0x2C: INR L */
   { 1, { 5, 5 }, { INSTR_INR } },
   /* 0x2D: DCR L */
   { 1, { 5, 5 }, { INSTR_DCR } },
   /* 0x2E: MVI L,d8 */
   { 2, { 7, 7 }, { INSTR_MVI } },
   /* 0x2F: CMA */
   { 1, { 4, 4 }, { INSTR_CMA } },
   /* 0x30: *NOP */
   { 1, { 4, 4 }, { INSTR_NOP } },
   /* 0x31: LXI SP,d16 */
   { 3, { 10, 10 }, { INSTR_LXI } },
   /* 0x32: STA a16 */
   { 3, { 13, 13 }, { INSTR_STA } },
   /* 0x33: INX SP */
   { 1, { 5, 5 }, { INSTR_INX } },
   /* 0x34: INR M */
   { 1, { 10, 10 }, { INSTR_INR } },
   /* 0x35: DCR M */
   { 1, { 10, 10 }, { INSTR_INR } },
   /* 0x36: MVI M,d8 */
   { 2, { 10, 10 }, { INSTR_MVI } },
   /* 0x37: STC */
   { 1, { 4, 4 }, { INSTR_STC } },
   /* 0x38: *NOP */
   { 1, { 4, 4 }, { INSTR_NOP } },
   /* 0x39: DAD SP */
   { 1, { 10, 10 }, { INSTR_DAD } },
   /* 0x3A: LDA a16 */
   { 3, { 13, 13 }, { INSTR_LDA } },
   /* 0x3B: DCX SP */
   { 1, { 5, 5 }, { INSTR_DCX } },
   /* 0x3C: INR A */
   { 1, { 5, 5 }, { INSTR_INR } },
   /* 0x3D: DCR A */
   { 1, { 5, 5 }, { INSTR_DCR } },
   /* 0x3E: MVI A,d8 */
   { 2, { 7, 7 }, { INSTR_MVI } },
   /* 0x3F: CMC */
   { 1, { 4, 4 }, { INSTR_CMC } },
   /* 0x40: MOV B,B */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x41: MOV B,C */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x42: MOV B,D */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x43: MOV B,E */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x45: MOV B,H */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x45: MOV B,L */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x46: MOV B,M */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x47: MOV B,A */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x48: MOV C,B */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x49: MOV C,C */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x4A: MOV C,D */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x4B: MOV C,E */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x4C: MOV C,H */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x4D: MOV C,L */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x4E: MOV C,M */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x4F: MOV C,A */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x50: MOV D,B */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x51: MOV D,C */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x52: MOV D,D */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x53: MOV D,E */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x55: MOV D,H */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x55: MOV D,L */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x56: MOV D,M */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x57: MOV D,A */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x58: MOV E,B */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x59: MOV E,C */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x5A: MOV E,D */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x5B: MOV E,E */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x5C: MOV E,H */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x5D: MOV E,L */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x5E: MOV E,M */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x5F: MOV E,A */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x60: MOV H,B */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x61: MOV H,C */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x62: MOV H,D */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x63: MOV H,E */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x65: MOV H,H */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x65: MOV H,L */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x66: MOV H,M */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x67: MOV H,A */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x68: MOV L,B */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x69: MOV L,C */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x6A: MOV L,D */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x6B: MOV L,E */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x6C: MOV L,H */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x6D: MOV L,L */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x6E: MOV L,M */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x6F: MOV L,A */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x70: MOV M,B */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x71: MOV M,C */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x72: MOV M,D */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x73: MOV M,E */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x75: MOV M,H */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x75: MOV M,L */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x76: HLT */
   { 1, { 7, 7 }, { INSTR_HLT } },
   /* 0x77: MOV M,A */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x78: MOV A,B */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x79: MOV A,C */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x7A: MOV A,D */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x7B: MOV A,E */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x7C: MOV A,H */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x7D: MOV A,L */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x7E: MOV A,M */
   { 1, { 7, 7 }, { INSTR_MOV } },
   /* 0x7F: MOV A,A */
   { 1, { 5, 5 }, { INSTR_MOV } },
   /* 0x80: ADD B */
   { 1, { 4, 4 }, { INSTR_ADD } },
   /* 0x81: ADD C */
   { 1, { 4, 4 }, { INSTR_ADD } },
   /* 0x82: ADD D */
   { 1, { 4, 4 }, { INSTR_ADD } },
   /* 0x83: ADD E */
   { 1, { 4, 4 }, { INSTR_ADD } },
   /* 0x84: ADD H */
   { 1, { 4, 4 }, { INSTR_ADD } },
   /* 0x85: ADD L */
   { 1, { 4, 4 }, { INSTR_ADD } },
   /* 0x86: ADD M */
   { 1, { 7, 7 }, { INSTR_ADD } },
   /* 0x87: ADD A */
   { 1, { 4, 4 }, { INSTR_ADD } },
   /* 0x88: ADC B */
   { 1, { 4, 4 }, { INSTR_ADC } },
   /* 0x89: ADC C */
   { 1, { 4, 4 }, { INSTR_ADC } },
   /* 0x8A: ADC D */
   { 1, { 4, 4 }, { INSTR_ADC } },
   /* 0x8B: ADC E */
   { 1, { 4, 4 }, { INSTR_ADC } },
   /* 0x8C: ADC H */
   { 1, { 4, 4 }, { INSTR_ADC } },
   /* 0x8D: ADC L */
   { 1, { 4, 4 }, { INSTR_ADC } },
   /* 0x8E: ADC M */
   { 1, { 7, 7 }, { INSTR_ADC } },
   /* 0x8F: ADC A */
   { 1, { 4, 4 }, { INSTR_ADC } },
   /* 0x90: SUB B */
   { 1, { 4, 4 }, { INSTR_SUB } },
   /* 0x91: SUB C */
   { 1, { 4, 4 }, { INSTR_SUB } },
   /* 0x92: SUB D */
   { 1, { 4, 4 }, { INSTR_SUB } },
   /* 0x93: SUB E */
   { 1, { 4, 4 }, { INSTR_SUB } },
   /* 0x94: SUB H */
   { 1, { 4, 4 }, { INSTR_SUB } },
   /* 0x95: SUB L */
   { 1, { 4, 4 }, { INSTR_SUB } },
   /* 0x96: SUB M */
   { 1, { 7, 7 }, { INSTR_SUB } },
   /* 0x97: SUB A */
   { 1, { 4, 4 }, { INSTR_SUB } },
   /* 0x98: SBB B */
   { 1, { 4, 4 }, { INSTR_SBB } },
   /* 0x99: SBB C */
   { 1, { 4, 4 }, { INSTR_SBB } },
   /* 0x9A: SBB D */
   { 1, { 4, 4 }, { INSTR_SBB } },
   /* 0x9B: SBB E */
   { 1, { 4, 4 }, { INSTR_SBB } },
   /* 0x9C: SBB H */
   { 1, { 4, 4 }, { INSTR_SBB } },
   /* 0x9D: SBB L */
   { 1, { 4, 4 }, { INSTR_SBB } },
   /* 0x9E: SBB M */
   { 1, { 7, 7 }, { INSTR_SBB } },
   /* 0x9F: SBB A */
   { 1, { 4, 4 }, { INSTR_SBB } },
   /* 0xA0: ANA B */
   { 1, { 4, 4 }, { INSTR_ANA } },
   /* 0xA1: ANA C */
   { 1, { 4, 4 }, { INSTR_ANA } },
   /* 0xA2: ANA D */
   { 1, { 4, 4 }, { INSTR_ANA } },
   /* 0xA3: ANA E */
   { 1, { 4, 4 }, { INSTR_ANA } },
   /* 0xA4: ANA H */
   { 1, { 4, 4 }, { INSTR_ANA } },
   /* 0xA5: ANA L */
   { 1, { 4, 4 }, { INSTR_ANA } },
   /* 0xA6: ANA M */
   { 1, { 7, 7 }, { INSTR_ANA } },
   /* 0xA7: ANA A */
   { 1, { 4, 4 }, { INSTR_ANA } },
   /* 0xA8: XRA B */
   { 1, { 4, 4 }, { INSTR_XRA } },
   /* 0xA9: XRA C */
   { 1, { 4, 4 }, { INSTR_XRA } },
   /* 0xAA: XRA D */
   { 1, { 4, 4 }, { INSTR_XRA } },
   /* 0xAB: XRA E */
   { 1, { 4, 4 }, { INSTR_XRA } },
   /* 0xAC: XRA H */
   { 1, { 4, 4 }, { INSTR_XRA } },
   /* 0xAD: XRA L */
   { 1, { 4, 4 }, { INSTR_XRA } },
   /* 0xAE: XRA M */
   { 1, { 7, 7 }, { INSTR_XRA } },
   /* 0xAF: XRA A */
   { 1, { 4, 4 }, { INSTR_XRA } },
   /* 0xB0: ORA B */
   { 1, { 4, 4 }, { INSTR_ORA } },
   /* 0xB1: ORA C */
   { 1, { 4, 4 }, { INSTR_ORA } },
   /* 0xB2: ORA D */
   { 1, { 4, 4 }, { INSTR_ORA } },
   /* 0xB3: ORA E */
   { 1, { 4, 4 }, { INSTR_ORA } },
   /* 0xB4: ORA H */
   { 1, { 4, 4 }, { INSTR_ORA } },
   /* 0xB5: ORA L */
   { 1, { 4, 4 }, { INSTR_ORA } },
   /* 0xB6: ORA M */
   { 1, { 7, 7 }, { INSTR_ORA } },
   /* 0xB7: ORA A */
   { 1, { 4, 4 }, { INSTR_ORA } },
   /* 0xB8: CMP B */
   { 1, { 4, 4 }, { INSTR_CMP } },
   /* 0xB9: CMP C */
   { 1, { 4, 4 }, { INSTR_CMP } },
   /* 0xBA: CMP D */
   { 1, { 4, 4 }, { INSTR_CMP } },
   /* 0xBB: CMP E */
   { 1, { 4, 4 }, { INSTR_CMP } },
   /* 0xBC: CMP H */
   { 1, { 4, 4 }, { INSTR_CMP } },
   /* 0xBD: CMP L */
   { 1, { 4, 4 }, { INSTR_CMP } },
   /* 0xBE: CMP M */
   { 1, { 7, 7 }, { INSTR_CMP } },
   /* 0xBF: CMP A */
   { 1, { 4, 4 }, { INSTR_CMP } },
   /* 0xC0: RNZ */
   { 1, { 5, 11 }, { INSTR_CMP } },
   /* 0xC1: POP B */
   { 1, { 10, 10 }, { INSTR_POP } },
   /* 0xC2: JNZ a16 */
   { 3, { 10, 10 }, { INSTR_JNZ } },
   /* 0xC3: JMP a16 */
   { 3, { 10, 10 }, { INSTR_JMP } },
   /* 0xC4: CNZ a16 */
   { 3, { 11, 17 }, { INSTR_CNZ } },
   /* 0xC5: PUSH B */
   { 1, { 11, 11 }, { INSTR_PUSH } },
   /* 0xC6: ADI d8 */
   { 2, { 7, 7 }, { INSTR_ADI } },
   /* 0xC7: RST 0 */
   { 1, { 11, 11 }, { INSTR_RST } },
   /* 0xC8: RZ */
   { 1, { 5, 11 }, { INSTR_RZ } },
   /* 0xC9: RET */
   { 1, { 10, 10 }, { INSTR_RET } },
   /* 0xCA: JZ a16 */
   { 3, { 10, 10 }, { INSTR_JZ } },
   /* 0xCB: *JMP a16 */
   { 3, { 10, 10 }, { INSTR_JMP } },
   /* 0xCC: CZ a16 */
   { 3, { 11, 17 }, { INSTR_CZ } },
   /* 0xCD: CALL a16 */
   { 3, { 17, 17 }, { INSTR_CALL } },
   /* 0xCE: ACI d8 */
   { 2, { 7, 7 }, { INSTR_ACI } },
   /* 0xCF: RST 1 */
   { 1, { 11, 11 }, { INSTR_RST } },
   /* 0xD0: RNC */
   { 1, { 5, 11 }, { INSTR_RNC } },
   /* 0xD1: POP D */
   { 1, { 10, 10 }, { INSTR_POP } },
   /* 0xD2: JNC a16 */
   { 3, { 10, 10 }, { INSTR_JNC } },
   /* 0xD3: OUT d8 */
   { 2, { 10, 10 }, { INSTR_OUT } },
   /* 0xD4: CNC a16 */
   { 3, { 11, 17 }, { INSTR_CNC } },
   /* 0xD5: PUSH D */
   { 1, { 11, 11 }, { INSTR_PUSH } },
   /* 0xD6: SUI d8 */
   { 2, { 7, 7 }, { INSTR_SUI } },
   /* 0xD7: RST 2 */
   { 1, { 11, 11 }, { INSTR_RST } },
   /* 0xD8: RC */
   { 1, { 5, 11 }, { INSTR_RC } },
   /* 0xD9: *RET */
   { 1, { 10, 10 }, { INSTR_RET } },
   /* 0xDA: JC a16 */
   { 3, { 10, 10 }, { INSTR_JC } },
   /* 0xDB: IN d8 */
   { 2, { 10, 10 }, { INSTR_IN } },
   /* 0xDC: CC a16 */
   { 3, { 11, 17 }, { INSTR_CC } },
   /* 0xDD: *CALL a16 */
   { 3, { 17, 17 }, { INSTR_CALL } },
   /* 0xDE: SBI d8 */
   { 2, { 7, 7 }, { INSTR_SBI } },
   /* 0xDF: RST 3 */
   { 1, { 11, 11 }, { INSTR_RST } },
   /* 0xE0: RPO */
   { 1, { 5, 11 }, { INSTR_RPO } },
   /* 0xE1: POP H */
   { 1, { 10, 10 }, { INSTR_POP } },
   /* 0xE2: JPO a16 */
   { 3, { 10, 10 }, { INSTR_JPO } },
   /* 0xE3: XTHL */
   { 1, { 18, 18 }, { INSTR_XTHL } },
   /* 0xE4: CPO a16 */
   { 3, { 11, 17 }, { INSTR_CPO } },
   /* 0xE5: PUSH H */
   { 1, { 11, 11 }, { INSTR_PUSH } },
   /* 0xE6: ANI d8 */
   { 2, { 7, 7 }, { INSTR_ANI } },
   /* 0xE7: RST 4 */
   { 1, { 11, 11 }, { INSTR_RST } },
   /* 0xE8: RPE */
   { 1, { 5, 11 }, { INSTR_RPE } },
   /* 0xE9: PCHL */
   { 1, { 5, 5 }, { INSTR_PCHL } },
   /* 0xEA: JPE a16 */
   { 3, { 10, 10 }, { INSTR_JPE } },
   /* 0xEB: XCHG */
   { 1, { 5, 5 }, { INSTR_XCHG } },
   /* 0xEC: CPE a16 */
   { 3, { 11, 17 }, { INSTR_CPE } },
   /* 0xED: *CALL a16 */
   { 3, { 17, 17 }, { INSTR_CALL } },
   /* 0xEE: XRI d8 */
   { 2, { 7, 7 }, { INSTR_XRI } },
   /* 0xEF: RST 5 */
   { 1, { 11, 11 }, { INSTR_RST } },
   /* 0xF0: RP */
   { 1, { 5, 11 }, { INSTR_RP } },
   /* 0xF1: POP PSW */
   { 1, { 10, 10 }, { INSTR_RP } },
   /* 0xF2: JP a16 */
   { 3, { 10, 10 }, { INSTR_JP } },
   /* 0xF3: DI */
   { 1, { 4, 4 }, { INSTR_RP } },
   /* 0xF4: CP a16 */
   { 3, { 11, 17 }, { INSTR_CP } },
   /* 0xF5: PUSH PSW */
   { 1, { 11, 11 }, { INSTR_PUSH } },
   /* 0xF6: ORI d8 */
   { 2, { 7, 7 }, { INSTR_PUSH } },
   /* 0xF7: RST 6 */
   { 1, { 11, 11 }, { INSTR_PUSH } },
   /* 0xF8: RM */
   { 1, { 5, 11 }, { INSTR_RM } },
   /* 0xF9: SPHL */
   { 1, { 5, 5 }, { INSTR_SPHL } },
   /* 0xFA: JM a16 */
   { 3, { 10, 10 }, { INSTR_JM } },
   /* 0xFB: EI */
   { 1, { 4, 4 }, { INSTR_EI } },
   /* 0xFC: CM a16 */
   { 3, { 11, 17 }, { INSTR_CM } },
   /* 0xFD: *CALL a16 */
   { 3, { 17, 17 }, { INSTR_CALL } },
   /* 0xFE: CPI d8 */
   { 2, { 7, 7 }, { INSTR_CPI } },
   /* 0xFF: RST 7 */
   { 1, { 11, 11 }, { INSTR_CPI } }
};




/* TODO: This should be moved to an actual memory layer */
#define MEM_SIZE 256
byte_t g_tempRam[MEM_SIZE];

void
CPU_Fetch(byte_t* opcode);

void 
CPU_Decode(byte_t opcode);

void
CPU_AdvancePC();

void
CPU_Execute();



struct instruction* g_currentInstruction = 0;
u64 g_cycleCount = 0;


#include <time.h>
bool
Test_ALU_Adder()
{
  int i;
  int failCount;

  fprintf(stderr, "Testing ALU_Adder...\n");
  failCount = 0;
  srand(time(0));
  for (i = 0;
       i < 100;
       ++i)
  {
    byte_t byte1     = rand() % 256;
    byte_t byte2     = rand() % 256;
    byte_t origByte1 = byte1;

    /* Test sum */
    u32    bigSum    = (u32)byte1 + (u32)byte2;
    byte_t targetSum = bigSum & 0xff;
    bool shouldTripAuxCarry =
      ((u32)(byte1 & 0x0f + byte2 & 0x0f) > (byte_t)(byte1 & 0x0f + byte2 & 0x0f));
    bool shouldTripCarry = bigSum > targetSum;

    ALU_Adder(&byte1, byte2);
    if (targetSum != byte1)
    {
      fprintf(stderr, "TEST FAILED: Sum is incorrect; %d != %d\n", byte1, targetSum);
      ++failCount;
    }
    if (shouldTripAuxCarry && !CPU_GetFlag(FLG_AUXCRY))
    {
      fprintf(stderr, "TEST FAILED: Auxiliary Carry flag should be set but is not\n");
      ++failCount;
    }
    if (shouldTripCarry && !CPU_GetFlag(FLG_CARRY))
    {
      fprintf(stderr, "TEST FAILED: Carry flag should be set but is not\n");
      ++failCount;
    }

    if (failCount)
    {
      fprintf(stderr, "byte1: %#2x\n", origByte1);
      fprintf(stderr, "byte2: %#2x\n", byte2);
      fprintf(stderr, "  sum: %#2x\n", byte1);
      fprintf(stderr, "ALU_Adder: Some tests failed!\n");
      return false;
    }
  }

  fprintf(stderr, "ALU_Adder: All tests passed!\n\n");
  return true;

  /*
  printf("\nStart byte1: %#2x\nStart byte2: %#2x\n", byte1, byte2);
  printf("\nTarget byte1: %#2x\n", (byte_t)(byte1 + byte2));
  ALU_Adder(&byte1, byte2);
  printf("Actual byte1: %#2x\n\n", byte1);

  printf("    Carry: %#2x\n", CPU_GetFlag(FLG_CARRY));
  printf("Aux Carry: %#2x\n", CPU_GetFlag(FLG_AUXCRY));
  */
}

bool
Test_CheckByteParity()
{
  int i;
  
  srand(time(0));

  for (i = 0;
       i < 100;
       ++i)
  {
    byte_t byte         = rand() % 256;
    bit_t  parity       = CheckByteParity(byte);
    bit_t  targetParity = 0;

    if (parity != targetParity)
    {
      fprintf(stderr, "CheckByteParity: Incorrect parity\n");
      return false;
    }
  }
  return true;
}

void
RunTests()
{
  fprintf(stderr, "\nRunning tests...\n\n");
  if (!Test_ALU_Adder()) return;
  if (!Test_CheckByteParity()) return;
}


int
main(int argc, char* argv[])
{
  printf("%d\n", argc);
  if (argc > 1 && (strncmp("test", argv[1], 4) == 0))
  {
    RunTests();
    return 0;
  }

  
  memory = g_tempRam;
  
  bool poweredOn = true;
  while (poweredOn)
  {
    byte_t opcode;
    CPU_Fetch(&opcode);
    CPU_Decode(opcode);
    CPU_AdvancePC();
    CPU_Execute();
  }

  return 0;
}


byte_t
Mem_ReadByte(word_t address)
{
  if (address > MEM_SIZE - 1)
  {
    // TODO: Out of range
    return 0;
  }

  return memory[address];
}

void
Mem_WriteByte(byte_t address, byte_t data)
{
  if (address > MEM_SIZE - 1)
  {
    // TODO: Out of range
    abort();
  }

  memory[address] = data;
}

byte_t*
Mem_GetBytePointer(word_t address)
{
  if (address > MEM_SIZE - 1)
  {
    // TODO: Out of range
    return 0;
  }

  return &memory[address];
}


inline void
CPU_Fetch(byte_t* opcode)
{
  *opcode = Mem_ReadByte(regs.PC);
}

inline void
CPU_Decode(byte_t opcode)
{
  // TODO: Maybe check for invalid opcodes?
  g_currentInstruction = &instruction_set[opcode];
}

inline void
CPU_AdvancePC()
{
  regs.PC += g_currentInstruction->byteCount;
}

void
CPU_Execute()
{
  /*
  u8 operandCount = g_currentInstruction->byteCount - 1;
  byte_t* opera
  g_currentInstruction->Execute(&memory[];
  */


  /*
    TODO: My God, this is ugly. Fix it?
  */
  struct execute_params* params = &g_currentInstruction->executeParams;
  switch (params->instructionType)
  {


  case INSTR_ACI:
    {
      Execute_ACI();
      break;
    }

  case INSTR_ADC:
    {
      byte_t addend;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | regs.L;
        addend = Mem_ReadByte(address);
      }
      else
      {
        addend = CPU_GetRegValue(params->regs[0]);
      }
      Execute_ADC(addend);
      break;
    }

  case INSTR_ADD:
    {
      byte_t addend;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | regs.L;
        addend = Mem_ReadByte(address);
      }
      else
      {
        addend = CPU_GetRegValue(params->regs[0]);
      }
      Execute_ADD(addend);
      break;
    }

  case INSTR_ADI:
    {
      Execute_ADI();
      break;
    }

  case INSTR_ANA:
    {
      Execute_ANA();
      break;
    }

  case INSTR_ANI:
    {
      Execute_ANI();
      break;
    }

  case INSTR_CALL:
    {
      Execute_CALL();
      break;
    }

  case INSTR_CC:
    {
      Execute_CC();
      break;
    }

  case INSTR_CM:
    {
      Execute_CM();
      break;
    }

  case INSTR_CMA:
    {
      Execute_CMA();
      break;
    }

  case INSTR_CMC:
    {
      Execute_CMC();
      break;
    }

  case INSTR_CMP:
    {
      Execute_CMP();
      break;
    }

  case INSTR_CNC:
    {
      Execute_CNC();
      break;
    }

  case INSTR_CNZ:
    {
      Execute_CNZ();
      break;
    }

  case INSTR_CP:
    {
      Execute_CP();
      break;
    }

  case INSTR_CPE:
    {
      Execute_CPE();
      break;
    }

  case INSTR_CPI:
    {
      Execute_CPI();
      break;
    }

  case INSTR_CPO:
    {
      Execute_CPO();
      break;
    }

  case INSTR_CZ:
    {
      Execute_CZ();
      break;
    }

  case INSTR_DAA:
    {
      Execute_DAA();
      break;
    }

  case INSTR_DAD:
    {
      Execute_DAD();
      break;
    }

  case INSTR_DCR:
    {
      byte_t* byte = 0;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | (regs.L);
        byte = Mem_GetBytePointer(address);
      }
      else
      {
        byte = CPU_GetRegPointer(params->regs[0]);
      }
      Execute_DCR(byte);
      break;
    }

  case INSTR_DCX:
    {
      Execute_DCX();
      break;
    }

  case INSTR_EI:
    {
      Execute_EI();
      break;
    }

  case INSTR_HLT:
    {
      Execute_HLT();
      break;
    }

  case INSTR_IN:
    {
      Execute_IN();
      break;
    }

  case INSTR_INR:
    {
      byte_t* byte = 0;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | (regs.L);
        byte = Mem_GetBytePointer(address);
      }
      else
      {
        byte = CPU_GetRegPointer(params->regs[0]);
      }
      Execute_INR(byte);
      break;
    }

  case INSTR_INX:
    {
      Execute_INX();
      break;
    }

  case INSTR_JC:
    {
      Execute_JC();
      break;
    }

  case INSTR_JM:
    {
      Execute_JM();
      break;
    }

  case INSTR_JMP:
    {
      Execute_JMP();
      break;
    }

  case INSTR_JNC:
    {
      Execute_JNC();
      break;
    }

  case INSTR_JNZ:
    {
      Execute_JNZ();
      break;
    }

  case INSTR_JP:
    {
      Execute_JP();
      break;
    }

  case INSTR_JPE:
    {
      Execute_JPE();
      break;
    }

  case INSTR_JPO:
    {
      Execute_JPO();
      break;
    }

  case INSTR_JZ:
    {
      Execute_JZ();
      break;
    }

  case INSTR_LDA:
    {
      Execute_LDA();
      break;
    }

  case INSTR_LDAX:
    {
      word_t address;  
      if (params->regPair == REGPAIR_BC)
      {
        address = ((regs.B << 8) | regs.C);
      }
      else
      {
        address = ((regs.D << 8) | regs.E);
      }
      Execute_LDAX(address);
      break;
    }

  case INSTR_LHLD:
    {
      Execute_LHLD();
      break;
    }

  case INSTR_LXI:
    {
      Execute_LXI();
      break;
    }

  case INSTR_MOV:
    {
      byte_t* dst;
      byte_t* src;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | (regs.L);
        dst = Mem_GetBytePointer(address);
        src = CPU_GetRegPointer(params->regs[1]);
      }
      else if (params->regs[1] == REG_M)
      {
        word_t address = (regs.H << 8) | (regs.L);
        dst = Mem_GetBytePointer(address);
        src = CPU_GetRegPointer(params->regs[0]);
      }
      else
      {
        dst = CPU_GetRegPointer(params->regs[0]);
        src = CPU_GetRegPointer(params->regs[1]);
      }
      Execute_MOV(dst, src);
      break;
    }

  case INSTR_MVI:
    {
      reg8_t* dest = CPU_GetRegPointer(params->regs[0]);
      byte_t data = Mem_ReadByte(regs.PC - 1);
      Execute_MVI(dest, data);
      break;
    }

  case INSTR_NOP:
    {
      Execute_NOP();
      break;
    }

  case INSTR_ORA:
    {
      Execute_ORA();
      break;
    }

  case INSTR_OUT:
    {
      Execute_OUT();
      break;
    }

  case INSTR_PCHL:
    {
      Execute_PCHL();
      break;
    }

  case INSTR_POP:
    {
      Execute_POP();
      break;
    }

  case INSTR_PUSH:
    {
      Execute_PUSH();
      break;
    }

  case INSTR_RAL:
    {
      Execute_RAL();
      break;
    }

  case INSTR_RAR:
    {
      Execute_RAR();
      break;
    }

  case INSTR_RC:
    {
      Execute_RC();
      break;
    }

  case INSTR_RET:
    {
      Execute_RET();
      break;
    }

  case INSTR_RLC:
    {
      Execute_RLC();
      break;
    }

  case INSTR_RM:
    {
      Execute_RM();
      break;
    }

  case INSTR_RNC:
    {
      Execute_RNC();
      break;
    }

  case INSTR_RP:
    {
      Execute_RP();
      break;
    }

  case INSTR_RPE:
    {
      Execute_RPE();
      break;
    }

  case INSTR_RPO:
    {
      Execute_RPO();
      break;
    }

  case INSTR_RRC:
    {
      Execute_RRC();
      break;
    }

  case INSTR_RST:
    {
      Execute_RST();
      break;
    }

  case INSTR_RZ:
    {
      Execute_RZ();
      break;
    }

  case INSTR_SBB:
    {
      Execute_SBB();
      break;
    }

  case INSTR_SBI:
    {
      Execute_SBI();
      break;
    }

  case INSTR_SHLD:
    {
      Execute_SHLD();
      break;
    }

  case INSTR_SPHL:
    {
      Execute_SPHL();
      break;
    }

  case INSTR_STA:
    {
      Execute_STA();
      break;
    }

  case INSTR_STAX:
    {
      word_t address;  
      if (params->regPair == REGPAIR_BC)
      {
        address = ((regs.B << 8) | regs.C);
      }
      else
      {
        address = ((regs.D << 8) | regs.E);
      }
      Execute_STAX(address);
      break;
    }

  case INSTR_STC:
    {
      Execute_STC();
      break;
    }

  case INSTR_SUB:
    {
      Execute_SUB();
      break;
    }

  case INSTR_SUI:
    {
      Execute_SUI();
      break;
    }

  case INSTR_XCHG:
    {
      Execute_XCHG();
      break;
    }

  case INSTR_XRA:
    {
      Execute_XRA();
      break;
    }

  case INSTR_XRI:
    {
      Execute_XRI();
      break;
    }

  case INSTR_XTHL:
    {
      Execute_XTHL();
      break;
    }

  default:
    {
      ErrorFatal(ERR_INVALID_INSTRUCTION);
    }

  }

  /* TODO: The CPU should "sleep" long enough to simulate the actual
     8080 CPU clock */
  g_cycleCount += g_currentInstruction->cycleCount[CYCLE_COUNT_SHORT];
}

