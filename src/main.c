/*
  Name: Driven
  Puprose: An Intel 8080 CPU emulator
*/

#include <stdbool.h>
#include <stdint.h>

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

typedef u8  byte_t;
typedef u16 word_t; 

typedef byte_t reg8_t;
typedef word_t reg16_t; 


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
struct registers regs = {};



#define CYCLE_COUNT_SHORT 0
#define CYCLE_COUNT_LONG  1

typedef 
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

  ExecuteProc executeProc;
  
};


/*
  The instruction set array is built so that the index equates to the
  specific opcode.

  This array was built from a reference at:
    https://pastraiser.com/cpu/i8080/i8080_opcodes.html
*/
struct instruction instruction_set[256] =
  {
   /* 0x00: NOP */
   { 1, { 4, 4 },  },

   /* 0x01: LXI B,d16 */
   { 3, { 10, 10} },

   /* 0x02: STAX B */
   { 1, { 7, 7} },

   /* 0x03: INX B */
   { 1, { 5, 5 } },

   /* 0x04: INR B */
   { 1, { 5, 5 } },

   /* 0x05: DCR B */
   { 1, { 5, 5 } },

   /* 0x06: MVI B,d8 */
   { 2, { 7, 7 } },

   /* 0x07: RLC */
   { 1, { 4, 4 } },

   /* 0x08: *NOP */
   { 1, { 4, 4 } },

   /* 0x09: DAD B */
   { 1, { 10, 10 } },

   /* 0x0A: LDAX B */
   { 1, { 7, 7 } },

   /* 0x0B: DCX B */
   { 1, { 5, 5 } },

   /* 0x0C: INR C */
   { 1, { 5, 5 } },

   /* 0x0D: DCR C */
   { 1, { 5, 5 } },

   /* 0x0E: MVI C,d8 */
   { 2, { 7, 7 } },

   /* 0x0F: RRC */
   { 1, { 4, 4 } },

   /* 0x10: *NOP */
   { 1, { 4, 4 } },

   /* 0x11: LXI D,d16 */
   { 3, { 10, 10 } },

   /* 0x12: STAX D */
   { 1, { 7, 7 } },

   /* 0x13: INX D */
   { 1, { 5, 5 } },

   /* 0x14: INR D */
   { 1, { 5, 5 } },

   /* 0x15: DCR D */
   { 1, { 5, 5 } },

   /* 0x16: MVI D,d8 */
   { 2, { 7, 7 } },

   /* 0x17: RAL */
   { 1, { 4, 4 } },

   /* 0x18: *NOP */
   { 1, { 4, 4 } },

   /* 0x19: DAD D */
   { 1, { 10, 10 } },

   /* 0x1A: LDAX D */
   { 1, { 7, 7 } },

   /* 0x1B: DCX D */
   { 1, { 5, 5 } },

   /* 0x1C: INR E */
   { 1, { 5, 5 } },

   /* 0x1D: DCR E */
   { 1, { 5, 5 } },

   /* 0x1E: MVI E,d8 */
   { 2, { 7, 7 } },

   /* 0x1F: RAR */
   { 1, { 4, 4 } },

   /* 0x20: *NOP */
   { 1, { 4, 4 } },

   /* 0x21: LXI H,d16 */
   { 3, { 10, 10 } },

   /* 0x22: SHLD a16 */
   { 3, { 16, 16 } },

   /* 0x23: INX H */
   { 1, { 5, 5 } },

   /* 0x24: INR H */
   { 1, { 5, 5 } },

   /* 0x25: DCR H */
   { 1, { 5, 5 } },

   /* 0x26: MVI H,d8 */
   { 2, { 7, 7 } },

   /* 0x27: DAA */
   { 1, { 4, 4 } },

   /* 0x28: *NOP */
   { 1, { 4, 4 } },

   /* 0x29: DAD H */
   { 1, { 10, 10 } },

   /* 0x2A: LHLD a16 */
   { 3, { 16, 16 } },

   /* 0x2B: DCX H */
   { 1, { 5, 5 } },

   /* 0x2C: INR L */
   { 1, { 5, 5 } },

   /* 0x2D: DCR L */
   { 1, { 5, 5 } },

   /* 0x2E: MVI L,d8 */
   { 2, { 7, 7 } },

   /* 0x2F: CMA */
   { 1, { 4, 4 } },

   /* 0x30: *NOP */
   { 1, { 4, 4 } },

   /* 0x31: LXI SP,d16 */
   { 3, { 10, 10 } },

   /* 0x32: STA a16 */
   { 3, { 13, 13 } },

   /* 0x33: INX SP */
   { 1, { 5, 5 } },

   /* 0x34: INR M */
   { 1, { 10, 10 } },

   /* 0x35: DCR M */
   { 1, { 10, 10 } },

   /* 0x36: MVI M,d8 */
   { 2, { 10, 10 } },

   /* 0x37: STC */
   { 1, { 4, 4 } },

   /* 0x38: *NOP */
   { 1, { 4, 4 } },

   /* 0x39: DAD SP */
   { 1, { 10, 10 } },

   /* 0x3A: LDA a16 */
   { 3, { 13, 13 } },

   /* 0x3B: DCX SP */
   { 1, { 5, 5 } },

   /* 0x3C: INR A */
   { 1, { 5, 5 } },

   /* 0x3D: DCR A */
   { 1, { 5, 5 } },

   /* 0x3E: MVI A,d8 */
   { 2, { 7, 7 } },

   /* 0x3F: CMC */
   { 1, { 4, 4 } },

   /* 0x40: MOV B,B */
   { 1, { 5, 5 } },

   /* 0x41: MOV B,C */
   { 1, { 5, 5 } },

   /* 0x42: MOV B,D */
   { 1, { 5, 5 } },

   /* 0x43: MOV B,E */
   { 1, { 5, 5 } },

   /* 0x45: MOV B,H */
   { 1, { 5, 5 } },

   /* 0x45: MOV B,L */
   { 1, { 5, 5 } },

   /* 0x46: MOV B,M */
   { 1, { 7, 7 } },

   /* 0x47: MOV B,A */
   { 1, { 5, 5 } },

   /* 0x48: MOV C,B */
   { 1, { 5, 5 } },

   /* 0x49: MOV C,C */
   { 1, { 5, 5 } },

   /* 0x4A: MOV C,D */
   { 1, { 5, 5 } },

   /* 0x4B: MOV C,E */
   { 1, { 5, 5 } },

   /* 0x4C: MOV C,H */
   { 1, { 5, 5 } },

   /* 0x4D: MOV C,L */
   { 1, { 5, 5 } },

   /* 0x4E: MOV C,M */
   { 1, { 7, 7 } },

   /* 0x4F: MOV C,A */
   { 1, { 5, 5 } },

   /* 0x50: MOV D,B */
   { 1, { 5, 5 } },

   /* 0x51: MOV D,C */
   { 1, { 5, 5 } },

   /* 0x52: MOV D,D */
   { 1, { 5, 5 } },

   /* 0x53: MOV D,E */
   { 1, { 5, 5 } },

   /* 0x55: MOV D,H */
   { 1, { 5, 5 } },

   /* 0x55: MOV D,L */
   { 1, { 5, 5 } },

   /* 0x56: MOV D,M */
   { 1, { 7, 7 } },

   /* 0x57: MOV D,A */
   { 1, { 5, 5 } },

   /* 0x58: MOV E,B */
   { 1, { 5, 5 } },

   /* 0x59: MOV E,C */
   { 1, { 5, 5 } },

   /* 0x5A: MOV E,D */
   { 1, { 5, 5 } },

   /* 0x5B: MOV E,E */
   { 1, { 5, 5 } },

   /* 0x5C: MOV E,H */
   { 1, { 5, 5 } },

   /* 0x5D: MOV E,L */
   { 1, { 5, 5 } },

   /* 0x5E: MOV E,M */
   { 1, { 7, 7 } },

   /* 0x5F: MOV E,A */
   { 1, { 5, 5 } },

   /* 0x60: MOV H,B */
   { 1, { 5, 5 } },

   /* 0x61: MOV H,C */
   { 1, { 5, 5 } },

   /* 0x62: MOV H,D */
   { 1, { 5, 5 } },

   /* 0x63: MOV H,E */
   { 1, { 5, 5 } },

   /* 0x65: MOV H,H */
   { 1, { 5, 5 } },

   /* 0x65: MOV H,L */
   { 1, { 5, 5 } },

   /* 0x66: MOV H,M */
   { 1, { 7, 7 } },

   /* 0x67: MOV H,A */
   { 1, { 5, 5 } },

   /* 0x68: MOV L,B */
   { 1, { 5, 5 } },

   /* 0x69: MOV L,C */
   { 1, { 5, 5 } },

   /* 0x6A: MOV L,D */
   { 1, { 5, 5 } },

   /* 0x6B: MOV L,E */
   { 1, { 5, 5 } },

   /* 0x6C: MOV L,H */
   { 1, { 5, 5 } },

   /* 0x6D: MOV L,L */
   { 1, { 5, 5 } },

   /* 0x6E: MOV L,M */
   { 1, { 7, 7 } },

   /* 0x6F: MOV L,A */
   { 1, { 5, 5 } },

   /* 0x70: MOV M,B */
   { 1, { 7, 7 } },

   /* 0x71: MOV M,C */
   { 1, { 7, 7 } },

   /* 0x72: MOV M,D */
   { 1, { 7, 7 } },

   /* 0x73: MOV M,E */
   { 1, { 7, 7 } },

   /* 0x75: MOV M,H */
   { 1, { 7, 7 } },

   /* 0x75: MOV M,L */
   { 1, { 7, 7 } },

   /* 0x76: HLT */
   { 1, { 7, 7 } },

   /* 0x77: MOV M,A */
   { 1, { 5, 5 } },

   /* 0x78: MOV A,B */
   { 1, { 5, 5 } },

   /* 0x79: MOV A,C */
   { 1, { 5, 5 } },

   /* 0x7A: MOV A,D */
   { 1, { 5, 5 } },

   /* 0x7B: MOV A,E */
   { 1, { 5, 5 } },

   /* 0x7C: MOV A,H */
   { 1, { 5, 5 } },

   /* 0x7D: MOV A,L */
   { 1, { 5, 5 } },

   /* 0x7E: MOV A,M */
   { 1, { 7, 7 } },

   /* 0x7F: MOV A,A */
   { 1, { 5, 5 } },

   /* 0x80: ADD B */
   { 1, { 4, 4 } },

   /* 0x81: ADD C */
   { 1, { 4, 4 } },

   /* 0x82: ADD D */
   { 1, { 4, 4 } },

   /* 0x83: ADD E */
   { 1, { 4, 4 } },

   /* 0x84: ADD H */
   { 1, { 4, 4 } },

   /* 0x85: ADD L */
   { 1, { 4, 4 } },

   /* 0x86: ADD M */
   { 1, { 7, 7 } },

   /* 0x87: ADD A */
   { 1, { 4, 4 } },

   /* 0x88: ADC B */
   { 1, { 4, 4 } },

   /* 0x89: ADC C */
   { 1, { 4, 4 } },

   /* 0x8A: ADC D */
   { 1, { 4, 4 } },

   /* 0x8B: ADC E */
   { 1, { 4, 4 } },

   /* 0x8C: ADC H */
   { 1, { 4, 4 } },

   /* 0x8D: ADC L */
   { 1, { 4, 4 } },

   /* 0x8E: ADC M */
   { 1, { 7, 7 } },

   /* 0x8F: ADC A */
   { 1, { 4, 4 } },

   /* 0x90: SUB B */
   { 1, { 4, 4 } },

   /* 0x91: SUB C */
   { 1, { 4, 4 } },

   /* 0x92: SUB D */
   { 1, { 4, 4 } },

   /* 0x93: SUB E */
   { 1, { 4, 4 } },

   /* 0x94: SUB H */
   { 1, { 4, 4 } },

   /* 0x95: SUB L */
   { 1, { 4, 4 } },

   /* 0x96: SUB M */
   { 1, { 7, 7 } },

   /* 0x97: SUB A */
   { 1, { 4, 4 } },

   /* 0x98: SBB B */
   { 1, { 4, 4 } },

   /* 0x99: SBB C */
   { 1, { 4, 4 } },

   /* 0x9A: SBB D */
   { 1, { 4, 4 } },

   /* 0x9B: SBB E */
   { 1, { 4, 4 } },

   /* 0x9C: SBB H */
   { 1, { 4, 4 } },

   /* 0x9D: SBB L */
   { 1, { 4, 4 } },

   /* 0x9E: SBB M */
   { 1, { 7, 7 } },

   /* 0x9F: SBB A */
   { 1, { 4, 4 } },

   /* 0xA0: ANA B */
   { 1, { 4, 4 } },

   /* 0xA1: ANA C */
   { 1, { 4, 4 } },

   /* 0xA2: ANA D */
   { 1, { 4, 4 } },

   /* 0xA3: ANA E */
   { 1, { 4, 4 } },

   /* 0xA4: ANA H */
   { 1, { 4, 4 } },

   /* 0xA5: ANA L */
   { 1, { 4, 4 } },

   /* 0xA6: ANA M */
   { 1, { 7, 7 } },

   /* 0xA7: ANA A */
   { 1, { 4, 4 } },

   /* 0xA8: XRA B */
   { 1, { 4, 4 } },

   /* 0xA9: XRA C */
   { 1, { 4, 4 } },

   /* 0xAA: XRA D */
   { 1, { 4, 4 } },

   /* 0xAB: XRA E */
   { 1, { 4, 4 } },

   /* 0xAC: XRA H */
   { 1, { 4, 4 } },

   /* 0xAD: XRA L */
   { 1, { 4, 4 } },

   /* 0xAE: XRA M */
   { 1, { 7, 7 } },

   /* 0xAF: XRA A */
   { 1, { 4, 4 } },

   /* 0xB0: ORA B */
   { 1, { 4, 4 } },

   /* 0xB1: ORA C */
   { 1, { 4, 4 } },

   /* 0xB2: ORA D */
   { 1, { 4, 4 } },

   /* 0xB3: ORA E */
   { 1, { 4, 4 } },

   /* 0xB4: ORA H */
   { 1, { 4, 4 } },

   /* 0xB5: ORA L */
   { 1, { 4, 4 } },

   /* 0xB6: ORA M */
   { 1, { 7, 7 } },

   /* 0xB7: ORA A */
   { 1, { 4, 4 } },

   /* 0xB8: CMP B */
   { 1, { 4, 4 } },

   /* 0xB9: CMP C */
   { 1, { 4, 4 } },

   /* 0xBA: CMP D */
   { 1, { 4, 4 } },

   /* 0xBB: CMP E */
   { 1, { 4, 4 } },

   /* 0xBC: CMP H */
   { 1, { 4, 4 } },

   /* 0xBD: CMP L */
   { 1, { 4, 4 } },

   /* 0xBE: CMP M */
   { 1, { 7, 7 } },

   /* 0xBF: CMP A */
   { 1, { 4, 4 } },

   /* 0xC0: RNZ */
   { 1, { 5, 11 } },

   /* 0xC1: POP B */
   { 1, { 10, 10 } },

   /* 0xC2: JNZ a16 */
   { 3, { 10, 10 } },

   /* 0xC3: JMP a16 */
   { 3, { 10, 10 } },

   /* 0xC4: CNZ a16 */
   { 3, { 11, 17 } },

   /* 0xC5: PUSH B */
   { 1, { 11, 11 } },

   /* 0xC6: ADI d8 */
   { 2, { 7, 7 } },

   /* 0xC7: RST 0 */
   { 1, { 11, 11 } },

   /* 0xC8: RZ */
   { 1, { 5, 11 } },

   /* 0xC9: RET */
   { 1, { 10, 10 } },

   /* 0xCA: JZ a16 */
   { 3, { 10, 10 } },

   /* 0xCB: *JMP a16 */
   { 3, { 10, 10 } },

   /* 0xCC: CZ a16 */
   { 3, { 11, 17 } },

   /* 0xCD: CALL a16 */
   { 3, { 17, 17 } },

   /* 0xCE: ACI d8 */
   { 2, { 7, 7 } },

   /* 0xCF: RST 1 */
   { 1, { 11, 11 } },

   /* 0xD0: RNC */
   { 1, { 5, 11 } },

   /* 0xD1: POP D */
   { 1, { 10, 10 } },

   /* 0xD2: JNC a16 */
   { 3, { 10, 10 } },

   /* 0xD3: OUT d8 */
   { 2, { 10, 10 } },

   /* 0xD4: CNC a16 */
   { 3, { 11, 17 } },

   /* 0xD5: PUSH D */
   { 1, { 11, 11 } },

   /* 0xD6: SUI d8 */
   { 2, { 7, 7 } },

   /* 0xD7: RST 2 */
   { 1, { 11, 11 } },

   /* 0xD8: RC */
   { 1, { 5, 11 } },

   /* 0xD9: *RET */
   { 1, { 10, 10 } },

   /* 0xDA: JC a16 */
   { 3, { 10, 10 } },

   /* 0xDB: IN d8 */
   { 2, { 10, 10 } },

   /* 0xDC: CC a16 */
   { 3, { 11, 17 } },

   /* 0xDD: *CALL a16 */
   { 3, { 17, 17 } },

   /* 0xDE: SBI d8 */
   { 2, { 7, 7 } },

   /* 0xDF: RST 3 */
   { 1, { 11, 11 } },

   /* 0xE0: RPO */
   { 1, { 5, 11 } },

   /* 0xE1: POP H */
   { 1, { 10, 10 } },

   /* 0xE2: JPO a16 */
   { 3, { 10, 10 } },

   /* 0xE3: XTHL */
   { 1, { 18, 18 } },

   /* 0xE4: CPO a16 */
   { 3, { 11, 17 } },

   /* 0xE5: PUSH H */
   { 1, { 11, 11 } },

   /* 0xE6: ANI d8 */
   { 2, { 7, 7 } },

   /* 0xE7: RST 4 */
   { 1, { 11, 11 } },

   /* 0xE8: RPE */
   { 1, { 5, 11 } },

   /* 0xE9: PCHL */
   { 1, { 5, 5 } },

   /* 0xEA: JPE a16 */
   { 3, { 10, 10 } },

   /* 0xEB: XCHG */
   { 1, { 5, 5 } },

   /* 0xEC: CPE a16 */
   { 3, { 11, 17 } },

   /* 0xED: *CALL a16 */
   { 3, { 17, 17 } },

   /* 0xEE: XRI d8 */
   { 2, { 7, 7 } },

   /* 0xEF: RST 5 */
   { 1, { 11, 11 } },

   /* 0xF0: RP */
   { 1, { 5, 11 } },

   /* 0xF1: POP PSW */
   { 1, { 10, 10 } },

   /* 0xF2: JP a16 */
   { 3, { 10, 10 } },

   /* 0xF3: DI */
   { 1, { 4, 4 } },

   /* 0xF4: CP a16 */
   { 3, { 11, 17 } },

   /* 0xF5: PUSH PSW */
   { 1, { 11, 11 } },

   /* 0xF6: ORI d8 */
   { 2, { 7, 7 } },

   /* 0xF7: RST 6 */
   { 1, { 11, 11 } },

   /* 0xF8: RM */
   { 1, { 5, 11 } },

   /* 0xF9: SPHL */
   { 1, { 5, 5 } },

   /* 0xFA: JM a16 */
   { 3, { 10, 10 } },

   /* 0xFB: EI */
   { 1, { 4, 4 } },

   /* 0xFC: CM a16 */
   { 3, { 11, 17 } },

   /* 0xFD: *CALL a16 */
   { 3, { 17, 17 } },

   /* 0xFE: CPI d8 */
   { 2, { 7, 7 } },

   /* 0xFF: RST 7 */
   { 1, { 11, 11 } }

};




/* TODO: This should be moved to an actual memory layer */
#define MEM_SIZE 256
byte_t g_tempRam[MEM_SIZE];

byte_t
Mem_ReadByte(word_t address);




void
CPU_Fetch(byte_t* opcode);

void 
CPU_Decode(byte_t opcode);

void
CPU_AdvacePC();

void
CPU_Execute();



struct instruction* g_currentInstruction = 0;
u64 g_cycleCount = 0;


int
main(/*int argc, char* argv[]*/)
{
  memory = g_tempRam;
  
  bool poweredOn = true;
  while (poweredOn)
  {
    byte_t opcode;
    CPU_Fetch(&opcode);
    CPU_Decode(opcode);
    CPU_AdvacePC();
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

  /* TODO: The CPU should "sleep" long enough to simulate the actual
     8080 CPU clock */
  g_cycleCount += g_currentInstruction->cycleCount[CYCLE_COUNT_SHORT];
}


void
InstructionImpl_MVI(reg8_t* dest, u8 data)
{
  *dest = data;
}
