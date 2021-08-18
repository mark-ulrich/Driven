#include "cpu.h"
#include "instructions.h"
#include "log.h"
#include "memory.h"


internal byte_t *memory;
internal struct registers regs;
internal struct instruction* g_currentInstruction = 0;
internal u64 g_cycleCount = 0;

#define FLIPENDIAN_WORD(w) ((w << 8) | (w >>8))
#define MAKEWORD(a,b)      ((a << 8) | (b))

internal void
CPU_SetProgramCounter(word_t address);


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

internal void
CPU_SetRegValue(reg8_t reg, byte_t value)
{
  *CPU_GetRegPointer(reg) = value;
}

internal reg16_t*
CPU_GetRegPairPointer(u8 regPair)
{
#ifdef _DEBUG
  Log_Debug("CPU_GetRegPairPointer: regPair=%u", regPair);
#endif
  switch (regPair)
  {
  case REGPAIR_BC:
    {
      return (reg16_t*)&regs.B;
    }

  case REGPAIR_DE:
    {
      return (reg16_t*)&regs.D;
    }

  case REGPAIR_HL:
    {
      return (reg16_t*)&regs.H;
    }

  case REGPAIR_PSW:
    {
      return (reg16_t*)&regs.A;
    }

  case REG_SP:
    {
      return (reg16_t*)&regs.SP;
    }

  case REG_PC:
    {
      return (reg16_t*)&regs.PC;
    }
    
  default:
    return 0;
  }
}

word_t
CPU_GetRegPairValue(u8 regPair)
{
#ifdef _DEBUG
  Log_Debug("CPU_GetRegPairValue: regPair=%u", regPair);
#endif
  return *CPU_GetRegPairPointer(regPair);
}

/*
  Get the value of a bit in the Flags register
*/
bit_t
CPU_GetFlag(u8 flagBit)
{
  return (regs.F & flagBit);
}

/*
  Set the value of a bit in the Flags register
*/
internal void
CPU_SetFlag(u8 flagBit, bit_t state)
{
  if (state == 0)
  {
    regs.F &= ~flagBit;
  }
  else
  {
    regs.F |= flagBit;
  }
}

/*
  Toggle a bit in the flags register
*/
internal void
CPU_ToggleFlag(u8 flagBit)
{
  /* TODO: This is bad. Fix it. */
  if (CPU_GetFlag(flagBit))
    CPU_SetFlag(flagBit, 0);
  else
    CPU_SetFlag(flagBit, 1);
}

/*
  CPU_GetOperandByte()

  Retrieves a single byte following the instruction opcode in memory.
 */
byte_t
CPU_GetOperandByte()
{
  return Mem_ReadByte(CPU_GetProgramCounter() - g_currentInstruction->byteCount + 1);
}

/*
  CPU_GetOperandWord()

  Retrieves two bytes following the instruction opcode in memory.
 */
word_t
CPU_GetOperandWord()
{
  return Mem_ReadWord(CPU_GetProgramCounter() - g_currentInstruction->byteCount + 1);
}

/*
  Attempt to implement a simple ALU Adder.

  TODO: This is super ugly. Do some research and fix this!
  
  TODO: Does this handle subtraction correctly? Specifically, are the
  flags set correctly? (Looking at you, CARRY
 */
internal void
ALU_Adder(byte_t* byte, u8 addend, u8 flagsAffected)
{
  u8     bitIndex;
  byte_t sum   = 0;
  bit_t  carry = 0;

#ifdef _DEBUG
      Log_Debug("ALU_Adder: Entering...");
#endif

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

#ifdef _DEBUG
      Log_Debug("ALU_Adder: bitIndex=%x", bitIndex);
#endif
    if (bitIndex == 3 && 
        (flagsAffected & FLG_AUXCRY))
    {
#ifdef _DEBUG
      Log_Debug("ALU_Adder: FLG_AUXCRY=%x", carry);
#endif
      CPU_SetFlag(FLG_AUXCRY, carry);
    }
  }
  *byte = sum;

#ifdef _DEBUG
      Log_Debug("ALU_Adder: FLG_CARRY=%x", carry);
      Log_Debug("ALU_Adder: FLG_SIGN=%x", IsSigned(*byte));
#endif
  if (flagsAffected & FLG_CARRY)
  {
#ifdef _DEBUG
      Log_Debug("ALU_Adder: flagsAffected | FLG_CARRY=%x", flagsAffected | FLG_CARRY);
#endif
    CPU_SetFlag(FLG_CARRY, carry);
  }
  if (flagsAffected & FLG_SIGN)
    CPU_SetFlag(FLG_SIGN, IsSigned(*byte));
  if (flagsAffected & FLG_ZERO)
    CPU_SetFlag(FLG_ZERO, *byte == 0);
  if (flagsAffected & FLG_PARITY)
    CPU_SetFlag(FLG_PARITY, CheckBitParity(*byte));
}

/*
  ALU_Subtract

  Perform a subtraction using two's complement arithmetic.
 */
internal void
ALU_Subtract(byte_t* byte, u8 subtrahend, u8 flags)
{
  subtrahend = ~subtrahend + 1;
  ALU_Adder(byte, subtrahend, flags);
  CPU_ToggleFlag(FLG_CARRY);
}

/*
  ALU_LogicalAND(void)

  Implement a logical AND operation
*/
internal void
ALU_LogicalAND(byte_t* byte, byte_t data, byte_t flags)
{
  *byte &= data;

  if (flags & FLG_ZERO && *byte == 0)
    CPU_SetFlag(FLG_ZERO, 1);
  if (flags & FLG_SIGN)
    CPU_SetFlag(FLG_SIGN, (*byte & 0x80) >> 7);
  if (flags & FLG_PARITY)
    CPU_SetFlag(FLG_PARITY, CheckBitParity(*byte));
}

/*
  ALU_LogicalXOR(void)

  Implement a logical EXCLUSIVE OR operation
*/
internal void
ALU_LogicalXOR(byte_t* byte, byte_t data, byte_t flags)
{
  *byte ^= data;

  if (flags & FLG_ZERO)
    CPU_SetFlag(FLG_ZERO, *byte == 0);
  if (flags & FLG_SIGN)
    CPU_SetFlag(FLG_SIGN, (*byte & 0x80) >> 7);
  if (flags & FLG_PARITY)
    CPU_SetFlag(FLG_PARITY, CheckBitParity(*byte));
  if (flags & FLG_CARRY)
    CPU_SetFlag(FLG_CARRY, 0);

  /* TODO: I'm unsure how the Auxiliary Carry bit should be
     affected; I'm assuming it is reset like the the Carry bit */
  if (flags & FLG_AUXCRY)
    CPU_SetFlag(FLG_AUXCRY, 0);
}

/*
  ALU_LogicalOR(void)

  Implement a logical OR operation
*/
internal void
ALU_LogicalOR(byte_t* byte, byte_t data, byte_t flags)
{
  *byte |= data;

  if (flags & FLG_ZERO)
    CPU_SetFlag(FLG_ZERO, *byte == 0);
  if (flags & FLG_SIGN)
    CPU_SetFlag(FLG_SIGN, (*byte & 0x80) >> 7);
  if (flags & FLG_PARITY)
    CPU_SetFlag(FLG_PARITY, CheckBitParity(*byte));
  if (flags & FLG_CARRY)
    CPU_SetFlag(FLG_CARRY, 0);

  /* TODO: I'm unsure how the Auxiliary Carry bit should be
     affected; I'm assuming it is reset like the the Carry bit */
  if (flags & FLG_AUXCRY)
    CPU_SetFlag(FLG_AUXCRY, 0);
}


/*
  ---------------------------------------------------------------------
                       Instruction Implementations
  ---------------------------------------------------------------------
 */


/*
  ACI - Add Immediate to Accumulator With Carry

  The byte of immediate data is added to the contents of the
  accumulator plus the contents of the Carry bit.
  
  Flags: C S Z P AC
*/
internal void
Execute_ACI(void)
{
  byte_t data;

  data = Mem_ReadByte(regs.PC - g_currentInstruction->byteCount + 1);
  data += CPU_GetFlag(FLG_CARRY);
  ALU_Adder(&regs.A, data, FLG_CARRY | FLG_SIGN | FLG_ZERO | FLG_PARITY | FLG_AUXCRY);
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
  addend += CPU_GetFlag(FLG_CARRY);
  ALU_Adder(&regs.A, addend, FLG_CARRY | FLG_AUXCRY | FLG_SIGN | FLG_ZERO | FLG_PARITY);
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
  ALU_Adder(&regs.A, addend, FLG_CARRY | FLG_AUXCRY | FLG_SIGN | FLG_ZERO | FLG_PARITY);
}

/*
  ADI - Add Immediate to Accumulator

  The byte of immediate data is added to the contents of the
  accumulator using two's complement arithmetic.
  
  Flags: C S Z P AC
*/
internal void
Execute_ADI(void)
{
  byte_t data;

  data = Mem_ReadByte(regs.PC - g_currentInstruction->byteCount + 1);
  ALU_Adder(&regs.A, data, FLG_CARRY | FLG_SIGN | FLG_ZERO | FLG_PARITY | FLG_AUXCRY);
}

/*
  ANA - Logical AND Register or Memory with Accumulator

  The specified byte is logically ANDed with the contents of the
  accumulator. The Carry flag is reset to zero.
  
  Flags: C Z S P
*/
internal void
Execute_ANA(byte_t data)
{
  ALU_LogicalAND(&regs.A, data, FLG_ZERO | FLG_SIGN | FLG_PARITY);
  CPU_SetFlag(FLG_CARRY, 0);
}

/*
  ANI - AND Immediate with Accumulator
  
  The byte of immediate data is logically ANDed with the contents of
  the accumulator. The Carry bit is reset to zero.
  
  Flags: C Z S P
*/
internal void
Execute_ANI(void)
{
  byte_t data;

  data = Mem_ReadByte(regs.PC - g_currentInstruction->byteCount + 1);
  ALU_LogicalAND(&regs.A, data, FLG_CARRY | FLG_ZERO | FLG_SIGN | FLG_PARITY);
  CPU_SetFlag(FLG_CARRY, 0);
}

/*
  CALL - Call

  A call operation is unconditionaly performed to the specified
  subroutine.

  Flags: None
*/
internal void
Execute_CALL(void)
{
  Execute_PUSH(CPU_GetProgramCounter());
  Execute_JMP();
}

/*
  CC - Call If Carry
  
  If the Carry bit is one, a call operation is performed to the
  specified subroutine.
  
  Flags: None
*/
internal void
Execute_CC(void)
{
  if (CPU_GetFlag(FLG_CARRY))
    Execute_CALL();
}

/*
  CM - Call If Minus
  
  If the Sign bit is one (indicating a minus result), a call operation
  is performed to the specified subroutine.
  
  Flags: None
*/
internal void
Execute_CM(void)
{
  if (CPU_GetFlag(FLG_SIGN))
    Execute_CALL();
}

/*
  CMA - Complement Accumulator

  Each it of the contents of the accumulator is complemented.

  Flags affected: None
 */
internal void
Execute_CMA(void)
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

/*
  CMP - Compare Register or Memory with Accumulator
  
  The specified byte is compared to the contents of the
  accumulator. The comparison is performed by internally subtracting
  the byte from the accumulator (leaving both *unchanged*) and setting
  the conditions according to the result. In particular, the Zero bit
  is set if the quantities are equal, and reset if they are
  unequal. The Carry bit will be set if there is no carry out of bit
  7, indicating that the data byte is greater than the contents of the
  accumulator, and reset otherwise.
  
  NOTE: If the two quantities to be compared differ in sign, the sense
  of the Carry bit is reversed.
  
  Flags: C Z S P AC
*/
internal void
Execute_CMP(byte_t data)
{
  byte_t temp;
  temp = regs.A;
  ALU_Subtract(&temp, data, FLG_CARRY | FLG_ZERO | FLG_SIGN | FLG_PARITY | FLG_AUXCRY);
}

/*
  CNC - Call If No Carry

  If the Carry bit is zero, a call operation is performed to the
  specified subroutine.
  
  Flags: None
*/
internal void
Execute_CNC(void)
{
  if (!CPU_GetFlag(FLG_ZERO))
    Execute_CALL();
}

/*
  CNZ - Call If Not Zero
  
  If the Zero bit is zero, a call operation is performed to the
  specified subroutine.
  
  Flags: None
*/
internal void
Execute_CNZ(void)
{
  if (!CPU_GetFlag(FLG_ZERO))
    Execute_CALL();
}

/*
  CP - Call If Plus
  
  If the Sign bit is zero (indicating a positive result), a call
  operation is performed to the specified subroutine.
  
  Flags: None
*/
internal void
Execute_CP(void)
{
  if (!CPU_GetFlag(FLG_SIGN))
    Execute_CALL();
}

/*
  CPE - Call If Parity Even
  
  If the Parity bit is one (indicating even parity), a call operation
  is performed to the specified subroutine.
  
  Flags: None
*/
internal void
Execute_CPE(void)
{
  if (CPU_GetFlag(FLG_PARITY))
    Execute_CALL();
}

/*
  CPI - Compare Immediate with Accumulator
  
  The byte of immediate data is compared to the contents of the
  accumulator.
  
  Flags: C Z S P AC
*/
internal void
Execute_CPI(void)
{
  byte_t data;
  byte_t temp;

  temp = regs.A;
  data = Mem_ReadByte(regs.PC - g_currentInstruction->byteCount + 1);

#ifdef _DEBUG
  Log_Debug("Execute_CPI: temp=0x%02x data=0x%02x", temp, data);
#endif
  ALU_Subtract(&temp, data, FLG_CARRY | FLG_ZERO | FLG_SIGN | FLG_PARITY | FLG_AUXCRY);
}

/*
  CPE - Call If Parity Odd
  
  If the Parity bit is zero (indicating odd parity), a call operation
  is performed to the specified subroutine.
  
  Flags: None
*/
internal void
Execute_CPO(void)
{
  if (!CPU_GetFlag(FLG_PARITY))
    Execute_CALL();
}

/*
  CZ - Call If Zero
  
  If the Zero bit is one, a call operation is performed to the
  specified subroutine.
  
  Flags: None
*/
internal void
Execute_CZ(void)
{
  if (CPU_GetFlag(FLG_ZERO))
    Execute_CALL();
}

/*
  DAA - Decimal Adjust Accumulator
  
  See the Intel 8080 Reference

  Flags affected: Z, S, P, C, AC
*/
internal void
Execute_DAA(void)
{
  if ((regs.A & 0x0f) > 9 ||
      CPU_GetFlag(FLG_AUXCRY))
  {
    ALU_Adder(&regs.A, 6, FLG_ZERO | FLG_SIGN | FLG_PARITY | FLG_CARRY | FLG_AUXCRY);
  }
  if (((regs.A & 0xf0) >> 4) > 9 ||
      CPU_GetFlag(FLG_CARRY))
  {
    ALU_Adder(&regs.A, (6 << 4), FLG_ZERO | FLG_SIGN | FLG_PARITY | FLG_CARRY);
  }
}

/*
  DAD - Double Add

  The 16-bit number in the specified register pair is added to the
  16-bit number held in the H and L registers using two's complement
  arithmetic. The result replaces the contents of the H and L
  registers.
  
  Flags: C
*/
internal void
Execute_DAD(word_t data)
{
  byte_t  srcHi,  srcLow;
  byte_t *dst;
  byte_t *dstHi, *dstLow;

#ifdef _DEBUG
  Log_Debug("Execute_DAD: data=0x%04x", data);
#endif
  srcLow = (byte_t)data;
  srcHi  = (byte_t)(data >> 8);
  dst = (byte_t*)CPU_GetRegPairPointer(REGPAIR_HL);
  dstLow = dst+1;
  dstHi  = dst;

  ALU_Adder(dstLow, srcLow, FLG_CARRY);
  *dstHi += CPU_GetFlag(FLG_CARRY);
  ALU_Adder(dstHi,  srcHi,  FLG_CARRY);
}

/*
  DCR - Decrement Register or Memory
  
  The specified register or memory bit is decremented by one.

  Flags affected: Z, S, P, AC
*/
internal void
Execute_DCR(byte_t* byte)
{
  ALU_Adder(byte, -1, FLG_ZERO | FLG_SIGN | FLG_PARITY | FLG_AUXCRY);
}

/*
  DCX - Decrement Register Pair
  
  The 16-bit number held in the specified register pair is decremented
  by one.

  Flags: None
*/
internal void
Execute_DCX(reg16_t reg)
{
  bit_t   savedCarry;
  word_t *dst;
  byte_t *byteHi, *byteLow;

#ifdef _DEBUG
  Log_Debug("Execute_DCX: reg=0x%02x", reg);
#endif
  dst     = CPU_GetRegPairPointer(reg);
  byteHi  = (byte_t*)dst;
  byteLow = byteHi+1;

  savedCarry = CPU_GetFlag(FLG_CARRY);

  /* TODO: Fix this dumb hack for when the case when SP is the operand
     to INX */
  if (reg == REG_SP)
    *dst = FLIPENDIAN_WORD(*dst);
  ALU_Subtract(byteLow, 1, FLG_CARRY);
  if (CPU_GetFlag(FLG_CARRY))
    ALU_Subtract(byteHi, 1, 0);
  /* TODO: See above about dumb hack... */
  if (reg == REG_SP)
    *dst = FLIPENDIAN_WORD(*dst);
  CPU_SetFlag(FLG_CARRY, savedCarry);
}

/*
  DI - Disable Interrupts

  Resets the INTE flip-flop, causing the CPU to ignore all interrupts.
  
  Flags: None
*/
internal void
Execute_DI(void)
{
  abort();
}

/*
  EI - Enable Interrupts

  Sets the INTE flip-flop, enabling the CPU to recognize and respond
  to interrupts.
  
  Flags: None
*/
internal void
Execute_EI(void)
{
  abort();
}

/*
  HLT - Halt
  
  The CPU enters the STOPPED state and no further activity takes place
  until an interrupt occurs.
  
  Flags: None
*/
internal void
Execute_HLT(void)
{
  abort();
}

/*
  IN - Input

  An eight-bit data byte is read from a specified input device and
  replaces the contents of the accumulator.
  
  Flags: None
*/
internal void
Execute_IN(void)
{
  abort();
}

/*
  INR - Increment Register or Memory
  
  The specified register or memory bit is incremented by one.

  Flags affected: Z, S, P, AC
*/
internal void
Execute_INR(byte_t* byte)
{
  ALU_Adder(byte, 1, FLG_ZERO | FLG_SIGN | FLG_PARITY | FLG_AUXCRY);
}

/*
  INX - Increment Register Pair
  
  The 16-bit number held in the specified register pair is incremented
  by one.

  Flags: None
*/
internal void
Execute_INX(reg16_t reg)
{
  bit_t   savedCarry;
  word_t *dst;
  byte_t *byteHi, *byteLow;

#ifdef _DEBUG
  Log_Debug("Execute_INX: reg=0x%02x", reg);
#endif
  dst     = CPU_GetRegPairPointer(reg);
  byteHi  = (byte_t*)dst;
  byteLow = byteHi+1;

  savedCarry = CPU_GetFlag(FLG_CARRY);

  /* TODO: Fix this dumb hack for when the case when SP is the operand
     to INX */
  if (reg == REG_SP)
    *dst = FLIPENDIAN_WORD(*dst);
  ALU_Adder(byteLow, 1, FLG_CARRY);
  if (CPU_GetFlag(FLG_CARRY))
    ALU_Adder(byteHi, 1, 0);
  /* TODO: See above about dumb hack... */
  if (reg == REG_SP)
    *dst = FLIPENDIAN_WORD(*dst);
  CPU_SetFlag(FLG_CARRY, savedCarry);
}

/*
  JC - Jump If Carry

  If the Carry bit is one, program execution continues at the specified
  memory address.
  
  Flags: None
*/
internal void
Execute_JC(void)
{
  if (CPU_GetFlag(FLG_CARRY))
    Execute_JMP();
}

/*
  JM - Jump If Minus

  If the Sign bit is one (indicating a negative result), program
  execution continues at the specified memory address.
  
  Flags: None
*/
internal void
Execute_JM(void)
{
  if (CPU_GetFlag(FLG_SIGN))
    Execute_JMP();
}

/*
  JMP - Jump

  Program execution continues unconditionally at the specified memory
  address.
  
  Flags affected: None
*/
internal void
Execute_JMP()
{
  CPU_SetProgramCounter(CPU_GetOperandWord());
}

/*
  JNC - Jump If No Carry

  If the Carry it is zero, program execution continues at the specified
  memory address.
  
  Flags: None
*/
internal void
Execute_JNC(void)
{
  if (!CPU_GetFlag(FLG_CARRY))
    Execute_JMP();
}

/*
  JNZ - Jump If Not Zero

  If the Zero bit is zero, program execution continues at the specified
  memory address.
  
  Flags: None
*/
internal void
Execute_JNZ(void)
{
  if (!CPU_GetFlag(FLG_ZERO))
    Execute_JMP();
}

/*
  JP - Jump If Positive

  If the Sign bit is zero (indicating a positive result), program
  execution continues at the specified memory address.
  
  Flags: None
*/
internal void
Execute_JP(void)
{
  if (!CPU_GetFlag(FLG_SIGN))
    Execute_JMP();
}

/*
  JPE - Jump If Parity Even

  If the Parity bit is one (indicating a result with even parity),
  program execution continues at the specified memory address.
  
  Flags: None
*/
internal void
Execute_JPE(void)
{
  if (CPU_GetFlag(FLG_PARITY))
    Execute_JMP();
}

/*
  JPO - Jump If Parity Odd

  If the Parity bit is zero (indicating a result with odd parity),
  program execution continues at the specified memory address.
  
  Flags: None
*/
internal void
Execute_JPO(void)
{
  if (!CPU_GetFlag(FLG_PARITY))
    Execute_JMP();
}

/*
  JZ - Jump If Zero

  If the Zero bit is one, program execution continues at the specified
  memory address.
  
  Flags: None
*/
internal void
Execute_JZ(void)
{
  if (CPU_GetFlag(FLG_ZERO))
    Execute_JMP();
}

/*
  LDA - Load Accumulator Direct

  The byte at the specified memory address replaces the contents of
  the accumulator.
  
  Flags: None
*/
internal void
Execute_LDA(void)
{
  CPU_SetRegValue(REG_A, Mem_ReadByte(CPU_GetOperandWord()));
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

/*
  LHLD - Load H and L Direct

  The byte at the specified memory address replaces the contents of
  the L register. The byte at the next higher memory address replaces
  the contents of the H register.
  
  Flags: None
*/
internal void
Execute_LHLD(void)
{
  word_t address;

  address = CPU_GetOperandWord();
#ifdef _DEBUG
  Log_Debug("Execute_LHLD: address=0x%04x", address);
#endif
  regs.L = Mem_ReadByte(address);
  regs.H = Mem_ReadByte(address+1);
}

/*
  LXI - Load Register Pair Immediate

  The third byte of the instruction is loaded into the first register
  of the specified pair, while the second byte of the instruction is
  loaded into the second register of the pair.
  
  Flags: None
*/
internal void
Execute_LXI(reg16_t reg, word_t data)
{
  byte_t *dstLow, *dstHi;

  dstHi  = (byte_t*)CPU_GetRegPairPointer(reg);
  dstLow = dstHi+1;

  if (reg == REG_SP)
    data = FLIPENDIAN_WORD(data);
  *dstHi  = (byte_t)(data >> 8);
  *dstLow = (byte_t)data;
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
  MVI - Move Immediate Data

  The byte of immediate data is stored in the specified register or
  memory byte.

  Flags affected: None
*/
internal void
Execute_MVI(reg8_t reg)
{
  byte_t* dst;
  byte_t data;
  
  data = Mem_ReadByte(regs.PC - 1);
  if (reg == REG_M)
    dst = Mem_GetBytePointer(MAKEWORD(regs.H, regs.L));
  else
    dst = CPU_GetRegPointer(reg);
  *dst = data;
}

/*
  NOP - No Operation

  Do nothing.
  
  Flags affected: None 
*/
internal void
Execute_NOP(void)
{
  /* NOPE */
  /* fprintf(stderr, "NOPE\n");  */
}

/*
  ORA - Logical OR Register or Memory with Accumulator
  
  The specified byte is logically ORed with the contents of the
  accumulator. The Carry bit is reset to zero.
  
  Flags: C Z S P
*/
internal void
Execute_ORA(byte_t data)
{
  ALU_LogicalOR(&regs.A, data, FLG_CARRY | FLG_SIGN | FLG_ZERO | FLG_PARITY);
}


/*
  ORI - OR Immediate with Accumulator
  
  The byte of immediate data is logically ORed with the contents of
  the accumulator. The Carry bit is reset to zero.
  
  Flags: C Z S P
*/
internal void
Execute_ORI(void)
{
  byte_t data;

  data = Mem_ReadByte(regs.PC - g_currentInstruction->byteCount + 1);
  ALU_LogicalOR(&regs.A, data, FLG_CARRY | FLG_ZERO | FLG_SIGN | FLG_PARITY);
  CPU_SetFlag(FLG_CARRY, 0);
}

/*
  OUT - Output

  The contents of the accumulator are sent to the specified output
  device.
  
  Flags: None
*/
internal void
Execute_OUT(void)
{
  abort();
}

/*
  PCHL - Load Program Counter

  The contents of the H register replace the most significant 8 bits
  of the program counter, and the contents of the L register replace
  the least significant 8 bits of the program counter.
  
  Flags: None
*/
internal void
Execute_PCHL(void)
{
  CPU_SetProgramCounter(MAKEWORD(CPU_GetRegValue(REG_H), CPU_GetRegValue(REG_L)));
}


/*
  POP - Pop Data Off Stack

  The contents of the specified register pair are restored from two
  bytes of memory indicated by the stack pointer. The byte of data at
  the memory address indicated by the stack pointer is loaded into the
  second register of the register pair; the byte of data at the
  address one greater than the address indicated by the stack pointer
  is loaded into the first register of the pair. The stack pointer is
  incremented by two.
  
  Flags: If register pair PSW is specified, all flags may be changed;
  otherwise, no flags are modified.
*/
internal void
Execute_POP(byte_t* regAddr)
{
  *(regAddr+1) = Mem_ReadByte(regs.SP);
  *regAddr     = Mem_ReadByte(regs.SP+1);
  regs.SP += 2;
}

/*
  PUSH - Push Data Onto Stack

  The contents of the specified register pair are saved in two bytes
  of memory indicated by the stack pointer. The contents of thefirst
  register are saved at the memory address one less than the address
  indicated by the stack pointer; the contents of the second register
  are saved at the address two less than the address indicated by the
  stack pointer. After the data has been saved, the stack pointer is
  decremented by two.
  
  Flags: None
*/
internal void
Execute_PUSH(reg16_t reg)
{
  byte_t *lowByte, *hiByte;

  hiByte  = (byte_t*)CPU_GetRegPairPointer(reg);
  lowByte = hiByte + 1;

#ifdef _DEBUG
  Log_Debug("Execute_PUSH: hiByte=0x%02x lowByte=0x%02x", *hiByte, *lowByte);
#endif

  Mem_WriteByte(regs.SP-1, *hiByte);
  Mem_WriteByte(regs.SP-2, *lowByte);
  regs.SP -= 2;
}

/*
  RAL - Rotate Accumulator Left Through Carry

  The contents of the accumulator are rotated one bit position to the
  left. The high-order bit of the accumulator replaces the Carry bit,
  while the Carry bit replaces the high-order bit of the accumulator.
  
  Flags: C
*/
internal void
Execute_RAL(void)
{
  bit_t highBit;

  highBit = GetBit(regs.A, 7);
#ifdef _DEBUG
  Log_Debug("Execute_RAL: highBit=%u", highBit);
#endif

  regs.A <<=  1;
  SetBit(&regs.A, 7, CPU_GetFlag(FLG_CARRY));
  CPU_SetFlag(FLG_CARRY, highBit);
}

/*
  RAR - Rotate Accumulator Right Through Carry

  The contents of the accumulator are rotated one bit position to the
  left. The low-order bit of the accumulator replaces the Carry bit,
  while the Carry bit replaces the high-order bit of the accumulator.
  
  Flags: C
*/
internal void
Execute_RAR(void)
{
  bit_t lowBit;

  lowBit = GetBit(regs.A, 0);
#ifdef _DEBUG
  Log_Debug("Execute_RAL: lowBit=%u", lowBit);
#endif

  regs.A >>=  1;
  SetBit(&regs.A, 7, CPU_GetFlag(FLG_CARRY));
  CPU_SetFlag(FLG_CARRY, lowBit);
}

/*
  RC - Return If Carry
  
  If the Carry bit is one, a return operation is performed.
  
  Flags: None
*/
internal void
Execute_RC(void)
{
  if (CPU_GetFlag(FLG_CARRY))
    Execute_RET();
}

/*
  RET - Return

  A return operation is unconditionally performed.

  Flags: None
*/
internal void
Execute_RET(void)
{
  word_t retAddress;
  retAddress = Mem_ReadWord(CPU_GetRegPairValue(REG_SP));
  CPU_SetProgramCounter(FLIPENDIAN_WORD(retAddress));
  regs.SP += 2;
}

/*
  RLC - Rotate Accumulator Left

  The Carry bit is set equal tot the high-order bit of the
  accumulator. The contents of the accumulator are rotated one bit
  position to the left, with the high-order bit being transferred to
  the low-order bit position of the accumulator.
  
  Flags: C
*/
internal void
Execute_RLC(void)
{
  bit_t highBit;

  highBit = GetBit(regs.A, 7);
#ifdef _DEBUG
  Log_Debug("Execute_RLC: highBit=%u", highBit);
#endif

  regs.A <<=  1;
  SetBit(&regs.A, 0, highBit);
  CPU_SetFlag(FLG_CARRY, highBit);
}

/*
  RM - Return If Minus
  
  If the Sign bit is one (indicating a minus result), a return
  operation is performed.
  
  Flags: None
*/
internal void
Execute_RM(void)
{
  if (CPU_GetFlag(FLG_SIGN))
    Execute_RET();
}

/*
  RNC - Return If No Carry
  
  If the Carry bit is zero, a return operation is performed.
  
  Flags: None
*/
internal void
Execute_RNC(void)
{
  if (!CPU_GetFlag(FLG_CARRY))
    Execute_RET();
}

/*
  RNZ - Return If Not Zero
  
  If the Zero bit is zero, a return operation is performed.
  
  Flags: None
*/
internal void
Execute_RNZ()
{
  if (!CPU_GetFlag(FLG_ZERO))
    Execute_RET();
}

/*
  RP - Return If Plus
  
  If the Sign bit is zero (indicating a positive result), a return
  operation is performed.
  
  Flags: None
*/
internal void
Execute_RP(void)
{
  if (!CPU_GetFlag(FLG_SIGN))
    Execute_RET();
}


/*
  RPE - Return If Parity Even
  
  If the Parity bit is one (indicating even parity), a return
  operation is performed.
  
  Flags: None
*/
internal void
Execute_RPE(void)
{
  if (CPU_GetFlag(FLG_PARITY))
    Execute_RET();
}

/*
  RPO - Return If Parity Odd
  
  If the Parity bit is zero (indicating odd parity), a return
  operation is performed.
  
  Flags: None
*/
internal void
Execute_RPO(void)
{
  if (!CPU_GetFlag(FLG_PARITY))
    Execute_RET();
}

/*
  RRC - Rotate Accumulator Right

  The Carry bit is set equal tot the low-order bit of the
  accumulator. The contents of the accumulator are rotated one bit
  position to the right, with the low-order bit being transferred to
  the high-order bit position of the accumulator.
  
  Flags: C
*/
internal void
Execute_RRC(void)
{
  bit_t lowBit;

  lowBit = GetBit(regs.A, 0);
#ifdef _DEBUG
  Log_Debug("Execute_RRC: lowBit=%u", lowBit);
#endif
  regs.A >>=  1;
  SetBit(&regs.A, 7, lowBit);
  CPU_SetFlag(FLG_CARRY, lowBit);
}

/*
  RST - Restart
  
  The contents of the program counter are pushed onto the stack,
  providing a return address for later use by a RETURN instruction.
  
  Flags: None
*/
internal void
Execute_RST(void)
{
  byte_t operand;
  word_t jumpAddress;
  
  operand = (CPU_GetOperandByte() & (7 << 3)) >> 3;
  jumpAddress = (operand << 4) | 0x000b;
  Execute_PUSH(CPU_GetProgramCounter());
  CPU_SetProgramCounter(jumpAddress);
}

/*
  RZ - Return If Zero
  
  If the Zero bit is one, a return operation is performed.
  
  Flags: None
*/
internal void
Execute_RZ(void)
{
  if (CPU_GetFlag(FLG_ZERO))
    Execute_RET();
}

/*
  SBB - Subtract Register or Memory From Accumulator with Borrow

  The carry bit is internally added to the contents of the specified
  byte; the value is then subtracted from the accumulator using two's
  compliment arithmetic.
  
  Flags affected: C S Z P AC
 */
internal void
Execute_SBB(byte_t subtrahend)
{
  subtrahend += CPU_GetFlag(FLG_CARRY);
  Execute_SUB(subtrahend);
}

/*
  SBI - Subtract Immediate from Accumulator with Borrow

  The Carry bit is internally added to the byte of immediate
  data. This value is then subtracted from the accumulator using two's
  complement arithmetic.
  
  Flags: C S Z P AC
*/
internal void
Execute_SBI(void)
{
  byte_t data;

  data = Mem_ReadByte(regs.PC - g_currentInstruction->byteCount + 1);
  data += CPU_GetFlag(FLG_CARRY);
  ALU_Subtract(&regs.A, data, FLG_CARRY | FLG_SIGN | FLG_ZERO | FLG_PARITY | FLG_AUXCRY);
}

/*
  SHLD - Store H and L Direct

  The contents of the L register are stored at the specfied memory
  address. The contents of the H register are stored at the next
  higher address.
  
  Flags: None
*/
internal void
Execute_SHLD(void)
{
  word_t address;

  address = CPU_GetOperandWord();
#ifdef _DEBUG
  Log_Debug("Execute_SHLD: address=0x%04x", address);
#endif
  Mem_WriteByte(address,   CPU_GetRegValue(REG_L));
  Mem_WriteByte(address+1, CPU_GetRegValue(REG_H));
}

/*
  SPHL - Load SP From H and L

  The 16 bits of data held in the H and L registers replace the
  contents of the stack pointer. The contents of the H and L registers
  are unchanged.
  
  Flags: None
*/
internal void
Execute_SPHL(void)
{
  regs.SP = MAKEWORD(regs.H, regs.L);
}

/*
  STA - Store Accumulator Direct

  The contents of the accumulator replace the byte at the specified
  memory address.
  
  Flags: None
*/
internal void
Execute_STA(void)
{
  Mem_WriteByte(CPU_GetOperandByte(), CPU_GetRegValue(REG_A));
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
Execute_STC(void)
{
  CPU_SetFlag(FLG_CARRY, 1);
}

/*
  SUB - Subtrace Register or Memory From Accumulator

  The specified byte is subtracted from the accumulator using two's
  compliment arithmetic.
  
  Flags affected: C S Z P AC
 */
internal void
Execute_SUB(byte_t subtrahend)
{
  ALU_Subtract(&regs.A, subtrahend, FLG_CARRY | FLG_SIGN | FLG_ZERO | FLG_PARITY | FLG_AUXCRY);
}


/*
  SUI - Subtract Immediate from Accumulator

  The byte of immediate data is subtracted from the contents of the
  accumulator using two's complement arithmetic.
  
  Flags: C S Z P AC
*/
internal void
Execute_SUI(void)
{
  byte_t data;

  data = Mem_ReadByte(regs.PC - g_currentInstruction->byteCount + 1);
  ALU_Subtract(&regs.A, data, FLG_CARRY | FLG_SIGN | FLG_ZERO | FLG_PARITY | FLG_AUXCRY);
}

/*
  Exchange Registers
  
  The 16 bits of data held in the H and L registers are exchanged with
  the 16 bits of data held in the D and E registers.
  
  Flags: None
*/
internal void
Execute_XCHG(void)
{
  SwapBytes(&regs.D, &regs.H);
  SwapBytes(&regs.E, &regs.L);
}

/*
  XRA - Logical XOR Register or Memory with Accumulator (Zero Accumulator)

  The specified byte is EXCLUSIVE ORed with the contents of the
  accumulator. The Carry flag is reset to zero.
  
  Flags: C Z S P
*/
internal void
Execute_XRA(byte_t data)
{
  ALU_LogicalXOR(&regs.A, data, FLG_CARRY | FLG_SIGN | FLG_ZERO | FLG_PARITY | FLG_AUXCRY);
}


/*
  XRI - EXCLUSIVE OR Immediate with Accumulator
  
  The byte of immediate data is EXCLUSIVE ORed with the contents of
  the accumulator. The Carry bit is reset to zero.
  
  Flags: C Z S P
*/
internal void
Execute_XRI(void)
{
  byte_t data;

  data = Mem_ReadByte(regs.PC - g_currentInstruction->byteCount + 1);
  ALU_LogicalXOR(&regs.A, data, FLG_CARRY | FLG_ZERO | FLG_SIGN | FLG_PARITY);
  CPU_SetFlag(FLG_CARRY, 0);
}

/*
  XTHL - Exchange Stack
  
  The contents of the L register are exchanged with the contents of
  the memory byte whose address is held in the stack pointer. The
  contents of the H register are exchanged with the contents of the
  memory byte whose address is one greater than that held in the stack
  pointer.
  
  Flags: None
 */
internal void
Execute_XTHL(void)
{
  SwapBytes(&regs.L, Mem_GetBytePointer(regs.SP));
  SwapBytes(&regs.H, Mem_GetBytePointer(regs.SP+1));
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
  { 1, { 4, 4 }, { INSTR_NOP, {} } },
  /* 0x01: LXI B,d16 */
  { 3, { 10, 10}, { INSTR_LXI, { REGPAIR_BC } } },
  /* 0x02: STAX B */
  { 1, { 7, 7},   { INSTR_STAX, { REGPAIR_BC } } },
  /* 0x03: INX B */
  { 1, { 5, 5 },  { INSTR_INX, { REGPAIR_BC } } },
  /* 0x04: INR B */
  { 1, { 5, 5 },  { INSTR_INR, { REG_B } } },
  /* 0x05: DCR B */
  { 1, { 5, 5 },  { INSTR_DCR, { REG_B } } },
  /* 0x06: MVI B,d8 */
  { 2, { 7, 7 },  { INSTR_MVI, { REG_B } } },
  /* 0x07: RLC */
  { 1, { 4, 4 }, { INSTR_RLC, {} } },
  /* 0x08: *NOP */
  { 1, { 4, 4 }, { INSTR_NOP, {} } },
  /* 0x09: DAD B */
  { 1, { 10, 10 }, { INSTR_DAD, { REGPAIR_BC } } },
  /* 0x0A: LDAX B */
  { 1, { 7, 7 }, { INSTR_LDAX, { REGPAIR_BC } } },
  /* 0x0B: DCX B */
  { 1, { 5, 5 }, { INSTR_DCX, { REGPAIR_BC } } },
  /* 0x0C: INR C */
  { 1, { 5, 5 }, { INSTR_INR, { REG_C } } },
  /* 0x0D: DCR C */
  { 1, { 5, 5 }, { INSTR_DCR, { REG_C } } },
  /* 0x0E: MVI C,d8 */
  { 2, { 7, 7 }, { INSTR_MVI, { REG_C } } },
  /* 0x0F: RRC */
  { 1, { 4, 4 }, { INSTR_RRC, {} } },
  /* 0x10: *NOP */
  { 1, { 4, 4 }, { INSTR_NOP, {} } },
  /* 0x11: LXI D,d16 */
  { 3, { 10, 10 }, { INSTR_LXI, { REGPAIR_DE } } },
  /* 0x12: STAX D */
  { 1, { 7, 7 }, { INSTR_STAX, { REGPAIR_DE } } },
  /* 0x13: INX D */
  { 1, { 5, 5 }, { INSTR_INX, { REGPAIR_DE } } },
  /* 0x14: INR D */
  { 1, { 5, 5 }, { INSTR_INR, { REG_D } } },
  /* 0x15: DCR D */
  { 1, { 5, 5 }, { INSTR_DCR, { REG_D } } },
  /* 0x16: MVI D,d8 */
  { 2, { 7, 7 }, { INSTR_MVI, { REG_D } } },
  /* 0x17: RAL */
  { 1, { 4, 4 }, { INSTR_RAL, {} } },
  /* 0x18: *NOP */
  { 1, { 4, 4 }, { INSTR_NOP, {} } },
  /* 0x19: DAD D */
  { 1, { 10, 10 }, { INSTR_DAD, { REGPAIR_DE } } },
  /* 0x1A: LDAX D */
  { 1, { 7, 7 }, { INSTR_LDAX, { REGPAIR_DE } } },
  /* 0x1B: DCX D */
  { 1, { 5, 5 }, { INSTR_DCX, { REGPAIR_DE } } },
  /* 0x1C: INR E */
  { 1, { 5, 5 }, { INSTR_INR, { REG_E } } },
  /* 0x1D: DCR E */
  { 1, { 5, 5 }, { INSTR_DCR, { REG_E } } },
  /* 0x1E: MVI E,d8 */
  { 2, { 7, 7 }, { INSTR_MVI, { REG_E } } },
  /* 0x1F: RAR */
  { 1, { 4, 4 }, { INSTR_RAR, {} } },
  /* 0x20: *NOP */
  { 1, { 4, 4 }, { INSTR_NOP, {} } },
  /* 0x21: LXI H,d16 */
  { 3, { 10, 10 }, { INSTR_LXI, { REGPAIR_HL } } },
  /* 0x22: SHLD a16 */
  { 3, { 16, 16 }, { INSTR_SHLD, {} } },
  /* 0x23: INX H */
  { 1, { 5, 5 }, { INSTR_INX, { REGPAIR_HL } } },
  /* 0x24: INR H */
  { 1, { 5, 5 }, { INSTR_INR, { REG_H } } },
  /* 0x25: DCR H */
  { 1, { 5, 5 }, { INSTR_DCR, { REG_H } } },
  /* 0x26: MVI H,d8 */
  { 2, { 7, 7 }, { INSTR_MVI, { REG_H } } },
  /* 0x27: DAA */
  { 1, { 4, 4 }, { INSTR_DAA, {} } },
  /* 0x28: *NOP */
  { 1, { 4, 4 }, { INSTR_NOP, {} } },
  /* 0x29: DAD H */
  { 1, { 10, 10 }, { INSTR_DAD, { REGPAIR_HL } } },
  /* 0x2A: LHLD a16 */
  { 3, { 16, 16 }, { INSTR_LHLD, {} } },
  /* 0x2B: DCX H */
  { 1, { 5, 5 }, { INSTR_DCX, { REGPAIR_HL } } },
  /* 0x2C: INR L */
  { 1, { 5, 5 }, { INSTR_INR, { REG_L } } },
  /* 0x2D: DCR L */
  { 1, { 5, 5 }, { INSTR_DCR, { REG_L } } },
  /* 0x2E: MVI L,d8 */
  { 2, { 7, 7 }, { INSTR_MVI, { REG_L } } },
  /* 0x2F: CMA */
  { 1, { 4, 4 }, { INSTR_CMA, {} } },
  /* 0x30: *NOP */
  { 1, { 4, 4 }, { INSTR_NOP, {} } },
  /* 0x31: LXI SP,d16 */
  { 3, { 10, 10 }, { INSTR_LXI, { REG_SP } } },
  /* 0x32: STA a16 */
  { 3, { 13, 13 }, { INSTR_STA, {} } },
  /* 0x33: INX SP */
  { 1, { 5, 5 }, { INSTR_INX, { REG_SP } } },
  /* 0x34: INR M */
  { 1, { 10, 10 }, { INSTR_INR, { REG_M } } },
  /* 0x35: DCR M */
  { 1, { 10, 10 }, { INSTR_DCR, { REG_M } } },
  /* 0x36: MVI M,d8 */
  { 2, { 10, 10 }, { INSTR_MVI, { REG_M } } },
  /* 0x37: STC */
  { 1, { 4, 4 }, { INSTR_STC, {} } },
  /* 0x38: *NOP */
  { 1, { 4, 4 }, { INSTR_NOP, {} } },
  /* 0x39: DAD SP */
  { 1, { 10, 10 }, { INSTR_DAD, { REG_SP } } },
  /* 0x3A: LDA a16 */
  { 3, { 13, 13 }, { INSTR_LDA, {} } },
  /* 0x3B: DCX SP */
  { 1, { 5, 5 }, { INSTR_DCX, { REG_SP } } },
  /* 0x3C: INR A */
  { 1, { 5, 5 }, { INSTR_INR, { REG_A } } },
  /* 0x3D: DCR A */
  { 1, { 5, 5 }, { INSTR_DCR, { REG_A } } },
  /* 0x3E: MVI A,d8 */
  { 2, { 7, 7 }, { INSTR_MVI, { REG_A } } },
  /* 0x3F: CMC */
  { 1, { 4, 4 }, { INSTR_CMC, {} } },
  /* 0x40: MOV B,B */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_B, REG_B } } },
  /* 0x41: MOV B,C */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_B, REG_C } } },
  /* 0x42: MOV B,D */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_B, REG_D } } },
  /* 0x43: MOV B,E */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_B, REG_E } } },
  /* 0x45: MOV B,H */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_B, REG_H } } },
  /* 0x45: MOV B,L */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_B, REG_L } } },
  /* 0x46: MOV B,M */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_B, REG_M } } },
  /* 0x47: MOV B,A */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_B, REG_A } } },
  /* 0x48: MOV C,B */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_C, REG_B } } },
  /* 0x49: MOV C,C */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_C, REG_C } } },
  /* 0x4A: MOV C,D */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_C, REG_D } } },
  /* 0x4B: MOV C,E */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_C, REG_E } } },
  /* 0x4C: MOV C,H */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_C, REG_H } } },
  /* 0x4D: MOV C,L */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_C, REG_L } } },
  /* 0x4E: MOV C,M */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_C, REG_M } } },
  /* 0x4F: MOV C,A */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_C, REG_A } } },
  /* 0x50: MOV D,B */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_D, REG_B } } },
  /* 0x51: MOV D,C */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_D, REG_C } } },
  /* 0x52: MOV D,D */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_D, REG_D } } },
  /* 0x53: MOV D,E */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_D, REG_E } } },
  /* 0x55: MOV D,H */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_D, REG_H } } },
  /* 0x55: MOV D,L */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_D, REG_L } } },
  /* 0x56: MOV D,M */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_D, REG_M } } },
  /* 0x57: MOV D,A */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_D, REG_A } } },
  /* 0x58: MOV E,B */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_E, REG_B } } },
  /* 0x59: MOV E,C */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_E, REG_C } } },
  /* 0x5A: MOV E,D */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_E, REG_D } } },
  /* 0x5B: MOV E,E */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_E, REG_E } } },
  /* 0x5C: MOV E,H */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_E, REG_H } } },
  /* 0x5D: MOV E,L */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_E, REG_L } } },
  /* 0x5E: MOV E,M */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_E, REG_M } } },
  /* 0x5F: MOV E,A */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_E, REG_A } } },
  /* 0x60: MOV H,B */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_H, REG_B } } },
  /* 0x61: MOV H,C */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_H, REG_C } } },
  /* 0x62: MOV H,D */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_H, REG_D } } },
  /* 0x63: MOV H,E */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_H, REG_E } } },
  /* 0x65: MOV H,H */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_H, REG_H } } },
  /* 0x65: MOV H,L */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_H, REG_L } } },
  /* 0x66: MOV H,M */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_H, REG_M } } },
  /* 0x67: MOV H,A */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_H, REG_A } } },
  /* 0x68: MOV L,B */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_L, REG_B } } },
  /* 0x69: MOV L,C */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_L, REG_C } } },
  /* 0x6A: MOV L,D */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_L, REG_D } } },
  /* 0x6B: MOV L,E */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_L, REG_E } } },
  /* 0x6C: MOV L,H */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_L, REG_H } } },
  /* 0x6D: MOV L,L */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_L, REG_L } } },
  /* 0x6E: MOV L,M */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_L, REG_M } } },
  /* 0x6F: MOV L,A */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_L, REG_A } } },
  /* 0x70: MOV M,B */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_M, REG_B } } },
  /* 0x71: MOV M,C */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_M, REG_C } } },
  /* 0x72: MOV M,D */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_M, REG_D } } },
  /* 0x73: MOV M,E */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_M, REG_E } } },
  /* 0x75: MOV M,H */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_M, REG_H } } },
  /* 0x75: MOV M,L */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_M, REG_L } } },
  /* 0x76: HLT */
  { 1, { 7, 7 }, { INSTR_HLT, {} } },
  /* 0x77: MOV M,A */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_M, REG_A } } },
  /* 0x78: MOV A,B */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_A, REG_B } } },
  /* 0x79: MOV A,C */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_A, REG_C } } },
  /* 0x7A: MOV A,D */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_A, REG_D } } },
  /* 0x7B: MOV A,E */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_A, REG_E } } },
  /* 0x7C: MOV A,H */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_A, REG_H } } },
  /* 0x7D: MOV A,L */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_A, REG_L } } },
  /* 0x7E: MOV A,M */
  { 1, { 7, 7 }, { INSTR_MOV, { REG_A, REG_M } } },
  /* 0x7F: MOV A,A */
  { 1, { 5, 5 }, { INSTR_MOV, { REG_A, REG_A } } },
  /* 0x80: ADD B */
  { 1, { 4, 4 }, { INSTR_ADD, { REG_B } } },
  /* 0x81: ADD C */
  { 1, { 4, 4 }, { INSTR_ADD, { REG_C } } },
  /* 0x82: ADD D */
  { 1, { 4, 4 }, { INSTR_ADD, { REG_D } } },
  /* 0x83: ADD E */
  { 1, { 4, 4 }, { INSTR_ADD, { REG_E } } },
  /* 0x84: ADD H */
  { 1, { 4, 4 }, { INSTR_ADD, { REG_H } } },
  /* 0x85: ADD L */
  { 1, { 4, 4 }, { INSTR_ADD, { REG_L } } },
  /* 0x86: ADD M */
  { 1, { 7, 7 }, { INSTR_ADD, { REG_M } } },
  /* 0x87: ADD A */
  { 1, { 4, 4 }, { INSTR_ADD, { REG_A } } },
  /* 0x88: ADC B */
  { 1, { 4, 4 }, { INSTR_ADC, { REG_B } } },
  /* 0x89: ADC C */
  { 1, { 4, 4 }, { INSTR_ADC, { REG_C } } },
  /* 0x8A: ADC D */
  { 1, { 4, 4 }, { INSTR_ADC, { REG_D } } },
  /* 0x8B: ADC E */
  { 1, { 4, 4 }, { INSTR_ADC, { REG_E } } },
  /* 0x8C: ADC H */
  { 1, { 4, 4 }, { INSTR_ADC, { REG_H } } },
  /* 0x8D: ADC L */
  { 1, { 4, 4 }, { INSTR_ADC, { REG_L } } },
  /* 0x8E: ADC M */
  { 1, { 7, 7 }, { INSTR_ADC, { REG_M } } },
  /* 0x8F: ADC A */
  { 1, { 4, 4 }, { INSTR_ADC, { REG_A } } },
  /* 0x90: SUB B */
  { 1, { 4, 4 }, { INSTR_SUB, { REG_B } } },
  /* 0x91: SUB C */
  { 1, { 4, 4 }, { INSTR_SUB, { REG_C } } },
  /* 0x92: SUB D */
  { 1, { 4, 4 }, { INSTR_SUB, { REG_D } } },
  /* 0x93: SUB E */
  { 1, { 4, 4 }, { INSTR_SUB, { REG_E } } },
  /* 0x94: SUB H */
  { 1, { 4, 4 }, { INSTR_SUB, { REG_H } } },
  /* 0x95: SUB L */
  { 1, { 4, 4 }, { INSTR_SUB, { REG_L } } },
  /* 0x96: SUB M */
  { 1, { 7, 7 }, { INSTR_SUB, { REG_M } } },
  /* 0x97: SUB A */
  { 1, { 4, 4 }, { INSTR_SUB, { REG_A } } },
  /* 0x98: SBB B */
  { 1, { 4, 4 }, { INSTR_SBB, { REG_B } } },
  /* 0x99: SBB C */
  { 1, { 4, 4 }, { INSTR_SBB, { REG_C } } },
  /* 0x9A: SBB D */
  { 1, { 4, 4 }, { INSTR_SBB, { REG_D } } },
  /* 0x9B: SBB E */
  { 1, { 4, 4 }, { INSTR_SBB, { REG_E } } },
  /* 0x9C: SBB H */
  { 1, { 4, 4 }, { INSTR_SBB, { REG_H } } },
  /* 0x9D: SBB L */
  { 1, { 4, 4 }, { INSTR_SBB, { REG_L } } },
  /* 0x9E: SBB M */
  { 1, { 7, 7 }, { INSTR_SBB, { REG_M } } },
  /* 0x9F: SBB A */
  { 1, { 4, 4 }, { INSTR_SBB, { REG_A } } },
  /* 0xA0: ANA B */
  { 1, { 4, 4 }, { INSTR_ANA, { REG_B } } },
  /* 0xA1: ANA C */
  { 1, { 4, 4 }, { INSTR_ANA, { REG_C } } },
  /* 0xA2: ANA D */
  { 1, { 4, 4 }, { INSTR_ANA, { REG_D } } },
  /* 0xA3: ANA E */
  { 1, { 4, 4 }, { INSTR_ANA, { REG_E } } },
  /* 0xA4: ANA H */
  { 1, { 4, 4 }, { INSTR_ANA, { REG_H } } },
  /* 0xA5: ANA L */
  { 1, { 4, 4 }, { INSTR_ANA, { REG_L } } },
  /* 0xA6: ANA M */
  { 1, { 7, 7 }, { INSTR_ANA, { REG_M } } },
  /* 0xA7: ANA A */
  { 1, { 4, 4 }, { INSTR_ANA, { REG_A } } },
  /* 0xA8: XRA B */
  { 1, { 4, 4 }, { INSTR_XRA, { REG_B } } },
  /* 0xA9: XRA C */
  { 1, { 4, 4 }, { INSTR_XRA, { REG_C } } },
  /* 0xAA: XRA D */
  { 1, { 4, 4 }, { INSTR_XRA, { REG_D } } },
  /* 0xAB: XRA E */
  { 1, { 4, 4 }, { INSTR_XRA, { REG_E } } },
  /* 0xAC: XRA H */
  { 1, { 4, 4 }, { INSTR_XRA, { REG_H } } },
  /* 0xAD: XRA L */
  { 1, { 4, 4 }, { INSTR_XRA, { REG_L } } },
  /* 0xAE: XRA M */
  { 1, { 7, 7 }, { INSTR_XRA, { REG_M } } },
  /* 0xAF: XRA A */
  { 1, { 4, 4 }, { INSTR_XRA, { REG_A } } },
  /* 0xB0: ORA B */
  { 1, { 4, 4 }, { INSTR_ORA, { REG_B } } },
  /* 0xB1: ORA C */
  { 1, { 4, 4 }, { INSTR_ORA, { REG_C } } },
  /* 0xB2: ORA D */
  { 1, { 4, 4 }, { INSTR_ORA, { REG_D } } },
  /* 0xB3: ORA E */
  { 1, { 4, 4 }, { INSTR_ORA, { REG_E } } },
  /* 0xB4: ORA H */
  { 1, { 4, 4 }, { INSTR_ORA, { REG_H } } },
  /* 0xB5: ORA L */
  { 1, { 4, 4 }, { INSTR_ORA, { REG_L } } },
  /* 0xB6: ORA M */
  { 1, { 7, 7 }, { INSTR_ORA, { REG_M } } },
  /* 0xB7: ORA A */
  { 1, { 4, 4 }, { INSTR_ORA, { REG_A } } },
  /* 0xB8: CMP B */
  { 1, { 4, 4 }, { INSTR_CMP, { REG_B } } },
  /* 0xB9: CMP C */
  { 1, { 4, 4 }, { INSTR_CMP, { REG_C } } },
  /* 0xBA: CMP D */
  { 1, { 4, 4 }, { INSTR_CMP, { REG_D } } },
  /* 0xBB: CMP E */
  { 1, { 4, 4 }, { INSTR_CMP, { REG_E } } },
  /* 0xBC: CMP H */
  { 1, { 4, 4 }, { INSTR_CMP, { REG_H } } },
  /* 0xBD: CMP L */
  { 1, { 4, 4 }, { INSTR_CMP, { REG_L } } },
  /* 0xBE: CMP M */
  { 1, { 7, 7 }, { INSTR_CMP, { REG_M } } },
  /* 0xBF: CMP A */
  { 1, { 4, 4 }, { INSTR_CMP, { REG_A } } },
  /* 0xC0: RNZ */
  { 1, { 5, 11 }, { INSTR_RNZ, {} } },
  /* 0xC1: POP B */
  { 1, { 10, 10 }, { INSTR_POP, { REGPAIR_BC } } },
  /* 0xC2: JNZ a16 */
  { 3, { 10, 10 }, { INSTR_JNZ, {} } },
  /* 0xC3: JMP a16 */
  { 3, { 10, 10 }, { INSTR_JMP, {} } },
  /* 0xC4: CNZ a16 */
  { 3, { 11, 17 }, { INSTR_CNZ, {} } },
  /* 0xC5: PUSH B */
  { 1, { 11, 11 }, { INSTR_PUSH, { REGPAIR_BC } } },
  /* 0xC6: ADI d8 */
  { 2, { 7, 7 }, { INSTR_ADI, {} } },
  /* 0xC7: RST 0 */
  { 1, { 11, 11 }, { INSTR_RST, {} } },
  /* 0xC8: RZ */
  { 1, { 5, 11 }, { INSTR_RZ, {} } },
  /* 0xC9: RET */
  { 1, { 10, 10 }, { INSTR_RET, {} } },
  /* 0xCA: JZ a16 */
  { 3, { 10, 10 }, { INSTR_JZ, {} } },
  /* 0xCB: *JMP a16 */
  { 3, { 10, 10 }, { INSTR_JMP, {} } },
  /* 0xCC: CZ a16 */
  { 3, { 11, 17 }, { INSTR_CZ, {} } },
  /* 0xCD: CALL a16 */
  { 3, { 17, 17 }, { INSTR_CALL, {} } },
  /* 0xCE: ACI d8 */
  { 2, { 7, 7 }, { INSTR_ACI, {} } },
  /* 0xCF: RST 1 */
  { 1, { 11, 11 }, { INSTR_RST, {} } },
  /* 0xD0: RNC */
  { 1, { 5, 11 }, { INSTR_RNC, {} } },
  /* 0xD1: POP D */
  { 1, { 10, 10 }, { INSTR_POP, { REGPAIR_DE } } },
  /* 0xD2: JNC a16 */
  { 3, { 10, 10 }, { INSTR_JNC, {} } },
  /* 0xD3: OUT d8 */
  { 2, { 10, 10 }, { INSTR_OUT, {} } },
  /* 0xD4: CNC a16 */
  { 3, { 11, 17 }, { INSTR_CNC, {} } },
  /* 0xD5: PUSH D */
  { 1, { 11, 11 }, { INSTR_PUSH, { REGPAIR_DE } } },
  /* 0xD6: SUI d8 */
  { 2, { 7, 7 }, { INSTR_SUI, {} } },
  /* 0xD7: RST 2 */
  { 1, { 11, 11 }, { INSTR_RST, {} } },
  /* 0xD8: RC */
  { 1, { 5, 11 }, { INSTR_RC, {} } },
  /* 0xD9: *RET */
  { 1, { 10, 10 }, { INSTR_RET, {} } },
  /* 0xDA: JC a16 */
  { 3, { 10, 10 }, { INSTR_JC, {} } },
  /* 0xDB: IN d8 */
  { 2, { 10, 10 }, { INSTR_IN, {} } },
  /* 0xDC: CC a16 */
  { 3, { 11, 17 }, { INSTR_CC, {} } },
  /* 0xDD: *CALL a16 */
  { 3, { 17, 17 }, { INSTR_CALL, {} } },
  /* 0xDE: SBI d8 */
  { 2, { 7, 7 }, { INSTR_SBI, {} } },
  /* 0xDF: RST 3 */
  { 1, { 11, 11 }, { INSTR_RST, {} } },
  /* 0xE0: RPO */
  { 1, { 5, 11 }, { INSTR_RPO, {} } },
  /* 0xE1: POP H */
  { 1, { 10, 10 }, { INSTR_POP, { REGPAIR_HL } } },
  /* 0xE2: JPO a16 */
  { 3, { 10, 10 }, { INSTR_JPO, {} } },
  /* 0xE3: XTHL */
  { 1, { 18, 18 }, { INSTR_XTHL, {} } },
  /* 0xE4: CPO a16 */
  { 3, { 11, 17 }, { INSTR_CPO, {} } },
  /* 0xE5: PUSH H */
  { 1, { 11, 11 }, { INSTR_PUSH, { REGPAIR_HL } } },
  /* 0xE6: ANI d8 */
  { 2, { 7, 7 }, { INSTR_ANI, {} } },
  /* 0xE7: RST 4 */
  { 1, { 11, 11 }, { INSTR_RST, {} } },
  /* 0xE8: RPE */
  { 1, { 5, 11 }, { INSTR_RPE, {} } },
  /* 0xE9: PCHL */
  { 1, { 5, 5 }, { INSTR_PCHL, {} } },
  /* 0xEA: JPE a16 */
  { 3, { 10, 10 }, { INSTR_JPE, {} } },
  /* 0xEB: XCHG */
  { 1, { 5, 5 }, { INSTR_XCHG, {} } },
  /* 0xEC: CPE a16 */
  { 3, { 11, 17 }, { INSTR_CPE, {} } },
  /* 0xED: *CALL a16 */
  { 3, { 17, 17 }, { INSTR_CALL, {} } },
  /* 0xEE: XRI d8 */
  { 2, { 7, 7 }, { INSTR_XRI, {} } },
  /* 0xEF: RST 5 */
  { 1, { 11, 11 }, { INSTR_RST, {} } },
  /* 0xF0: RP */
  { 1, { 5, 11 }, { INSTR_RP, {} } },
  /* 0xF1: POP PSW */
  { 1, { 10, 10 }, { INSTR_POP, { REGPAIR_PSW } } },
  /* 0xF2: JP a16 */
  { 3, { 10, 10 }, { INSTR_JP, {} } },
  /* 0xF3: DI */
  { 1, { 4, 4 }, { INSTR_DI, {} } },
  /* 0xF4: CP a16 */
  { 3, { 11, 17 }, { INSTR_CP, {} } },
  /* 0xF5: PUSH PSW */
  { 1, { 11, 11 }, { INSTR_PUSH, { REGPAIR_PSW } } },
  /* 0xF6: ORI d8 */
  { 2, { 7, 7 }, { INSTR_ORI, {} } },
  /* 0xF7: RST 6 */
  { 1, { 11, 11 }, { INSTR_RST, {} } },
  /* 0xF8: RM */
  { 1, { 5, 11 }, { INSTR_RM, {} } },
  /* 0xF9: SPHL */
  { 1, { 5, 5 }, { INSTR_SPHL, {} } },
  /* 0xFA: JM a16 */
  { 3, { 10, 10 }, { INSTR_JM, {} } },
  /* 0xFB: EI */
  { 1, { 4, 4 }, { INSTR_EI, {} } },
  /* 0xFC: CM a16 */
  { 3, { 11, 17 }, { INSTR_CM, {} } },
  /* 0xFD: *CALL a16 */
  { 3, { 17, 17 }, { INSTR_CALL, {} } },
  /* 0xFE: CPI d8 */
  { 2, { 7, 7 }, { INSTR_CPI, {} } },
  /* 0xFF: RST 7 */
  { 1, { 11, 11 }, { INSTR_RST, {} } }
};



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
CPU_AdvancePC(void)
{
  regs.PC += g_currentInstruction->byteCount;
}

void
CPU_Execute(void)
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
      byte_t data;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | regs.L;
        data = Mem_ReadByte(address);
      }
      else
      {
        data = CPU_GetRegValue(params->regs[0]);
      }
      Execute_ANA(data);
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
      byte_t data;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | regs.L;
        data = Mem_ReadByte(address);
      }
      else
      {
        data = CPU_GetRegValue(params->regs[0]);
      }
      Execute_CMP(data);
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
      word_t data;
      data = CPU_GetRegPairValue(params->regPair);
      /* TODO: Another stupid hack to deal the the SP case */
      if (params->regPair != REG_SP)
        data = FLIPENDIAN_WORD(data);
      Execute_DAD(data);
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

  case INSTR_DI:
    {
      Execute_DI();
      break;
    }

  case INSTR_DCX:
    {
      Execute_DCX(params->regPair);
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
      Execute_INX(params->regPair);
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
      word_t data;
      data = Mem_ReadWord(regs.PC - g_currentInstruction->byteCount + 1);
      Execute_LXI(params->regPair, data);
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
      Execute_MVI(params->regs[0]);
      break;
    }

  case INSTR_NOP:
    {
      Execute_NOP();
      break;
    }

  case INSTR_ORA:
    {
      byte_t data;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | regs.L;
        data = Mem_ReadByte(address);
      }
      else
      {
        data = CPU_GetRegValue(params->regs[0]);
      }
      Execute_ORA(data);
      break;
    }

  case INSTR_ORI:
    {
      Execute_ORI();
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
      byte_t* regAddr;
      regAddr = (reg8_t*)CPU_GetRegPairPointer(params->regPair);
      Execute_POP(regAddr);
      break;
    }

  case INSTR_PUSH:
    {
      Execute_PUSH(params->regPair);
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

  case INSTR_RNZ:
    {
      Execute_RNZ();
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
      byte_t subtrahend;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | regs.L;
        subtrahend = Mem_ReadByte(address);
      }
      else
      {
        subtrahend = CPU_GetRegValue(params->regs[0]);
      }
      Execute_SBB(subtrahend);
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
      byte_t subtrahend;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | regs.L;
        subtrahend = Mem_ReadByte(address);
      }
      else
      {
        subtrahend = CPU_GetRegValue(params->regs[0]);
      }
      Execute_SUB(subtrahend);
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
      byte_t data;
      if (params->regs[0] == REG_M)
      {
        word_t address = (regs.H << 8) | regs.L;
        data = Mem_ReadByte(address);
      }
      else
      {
        data = CPU_GetRegValue(params->regs[0]);
      }
      Execute_XRA(data);
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

void
CPU_Init(byte_t* memoryBlock)
{
  memory = memoryBlock;
  regs.F  = 0x02;
  regs.SP = 0x100;
}

void
CPU_DoInstructionCycle(void)
{
  byte_t opcode;
  CPU_Fetch(&opcode);
  CPU_Decode(opcode);
  CPU_AdvancePC();
  CPU_Execute();
}

reg16_t
CPU_GetProgramCounter(void)
{
  return (word_t)regs.PC;
}

void
CPU_SetProgramCounter(word_t address)
{
  regs.PC = address;
}

reg16_t
CPU_GetStackPointer(void)
{
  return (word_t)regs.SP;
}
