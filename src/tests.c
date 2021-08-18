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
  RunTests();
  return 0;

  return 0;
}
