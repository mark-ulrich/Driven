#ifndef __INSTRUCTIONS_H__
#define __INSTRUCTIONS_H__

#include "types.h"

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
      INSTR_DI,
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
      INSTR_ORI,
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
      INSTR_RNZ,
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
Execute_ACI();

internal void
Execute_ADC(byte_t addend);

internal void
Execute_ADD(byte_t addend);

internal void
Execute_ADI();

internal void
Execute_ANA(byte_t data);

internal void
Execute_ANI();

internal void
Execute_CALL();

internal void
Execute_CC();

internal void
Execute_CM();

internal void
Execute_CMA();

internal void
Execute_CMC(void);

internal void
Execute_CMP(byte_t data);

internal void
Execute_CNC();

internal void
Execute_CNZ();

internal void
Execute_CP();

internal void
Execute_CPE();

internal void
Execute_CPI();

internal void
Execute_CPO();

internal void
Execute_CZ();

internal void
Execute_DAA();

internal void
Execute_DAD(word_t data);

internal void
Execute_DCR(byte_t* byte);

internal void
Execute_DCX(reg16_t reg);

internal void
Execute_DI();

internal void
Execute_EI();

internal void
Execute_HLT();

internal void
Execute_IN();

internal void
Execute_INR(byte_t* byte);

internal void
Execute_INX(reg16_t reg);

internal void
Execute_JC();

internal void
Execute_JM();

internal void
Execute_JMP();

internal void
Execute_JNC();

internal void
Execute_JNZ();

internal void
Execute_JP();

internal void
Execute_JPE();

internal void
Execute_JPO();

internal void
Execute_JZ();

internal void
Execute_LDA();

internal void
Execute_LDAX(word_t address);

internal void
Execute_LHLD();

internal void
Execute_LXI(reg16_t reg, word_t data);

internal void
Execute_MOV(byte_t* dst, byte_t* src);

internal void
Execute_MVI(reg8_t reg);

internal void
Execute_NOP();

internal void
Execute_ORA(byte_t data);

internal void
Execute_ORI(void);

internal void
Execute_OUT();

internal void
Execute_PCHL();

internal void
Execute_POP(byte_t* regAddr);

internal void
Execute_PUSH(reg16_t reg);

internal void
Execute_RAL();

internal void
Execute_RAR();

internal void
Execute_RC();

internal void
Execute_RET();

internal void
Execute_RLC();

internal void
Execute_RM();

internal void
Execute_RNC();

internal void
Execute_RP();

internal void
Execute_RPE();

internal void
Execute_RPO();

internal void
Execute_RRC();

internal void
Execute_RST();

internal void
Execute_RZ();

internal void
Execute_SBB(byte_t subtrahend);

internal void
Execute_SBI();

internal void
Execute_SHLD();

internal void
Execute_SPHL();

internal void
Execute_STA();

internal void
Execute_STAX(byte_t address);

internal void
Execute_STC();

internal void
Execute_SUB(byte_t subtrahend);

internal void
Execute_SUI();

internal void
Execute_XCHG();

internal void
Execute_XRA(byte_t data);

internal void
Execute_XRI();

internal void
Execute_XTHL();

#endif    /* __INSTRUCTIONS_H__ */
