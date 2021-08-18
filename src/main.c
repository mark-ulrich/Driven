/*
  Name: Driven
  Puprose: An Intel 8080 CPU emulator
*/

#include "common.h"
#include "cpu.h"
#include "log.h"
#include "memory.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


void
PrintUsage(char* exeName)
{
  fprintf(stderr, "%s program\n", exeName);
}

bool
CharInString(char* haystack, char needle)
{
  while (*haystack)
  {
    if (needle == *haystack)
      return true;
    ++haystack;
  }
  return false;
}

void
ReplaceCharInString(char* s, char target, char sub)
{

  /*
#ifdef _DEBUG
  Log_Debug("ReplaceCharInString: s=\"%s\"", s);
#endif
  */

  while (*s)
  {
    /*
#ifdef _DEBUG
    Log_Debug("ReplaceCharInString: *s='%c'", *s);
#endif
    */

    if (*s == target)
      *s = sub;
    ++s;
  }
}

void
StripWhiteSpace(char* cmdString)
{
  int i;
  int dstIndex;
  int srcIndex;
  
  /* strip back */
  i = strlen(cmdString) - 1;
  while (cmdString[i] == ' ' ||
         cmdString[i] == '\t')
    --i;
  cmdString[i+1] = '\0';
  
  /* find first non-whitespace char */
  for (i = 0;
       cmdString[i] == ' ' ||
         cmdString[i] == '\t';
       ++i);
  srcIndex = i;

  /* move string to front of array */
  dstIndex = 0;
  while (cmdString[srcIndex])
  {
    cmdString[dstIndex] = cmdString[srcIndex];
    ++dstIndex;
    ++srcIndex;
  }
  cmdString[dstIndex] = '\0';
}

bool
FuzzyCompare(char* s1, char* s2)
{
  u32 testIndex, targetIndex;
  char* target;
  char* test;

  if (strlen(s2) > strlen(s1))
  {
    test   = s1;
    target = s2;
  }
  else
  {
    test   = s2;
    target = s1;
  }

  if (strlen(test) > strlen(target))
  {
    /* Log("FuzzyCompare: Too Long"); */
    return false;
  }
  else if (strlen(test) == strlen(target))
  {  
    /* Log("FuzzyCompare: Same Length"); */
    return !strcmp(test, target);
  }

  testIndex = targetIndex = 0;
  for (testIndex = 0;
       testIndex < strlen(test);
       ++testIndex)
  {
    while (test[testIndex] != target[targetIndex++])
    {
      if (!target[targetIndex])
        return false;
    }
  }

  return true;
}




#define ERRDBG_LOADPROGRAMFAILED   0xdeadbabe


enum
{
  DBGCMD_HELP,
  DBGCMD_NEXT,
  DBGCMD_QUIT,
  DBGCMD_STEP,
  DBGCMD_X,

  DBGCMD_NOCMD,
  DBGCMD_NUMCMDS
};


#define MAX_PARM_LENGTH       64
#define MAX_DBGCMD_PARMS       4

#define DBGCMD_FLG_HASPARMS    1
#define DBGCMD_FLG_HASEXPARMS  2

struct dbg_cmd
{
  char*     cmdName; 
  word_t    cmdID;
  char      parms[MAX_DBGCMD_PARMS][MAX_PARM_LENGTH];
  word_t    flags;
};

struct dbg_cmd DebuggerCommands[DBGCMD_NUMCMDS] =
{
  { "help",           DBGCMD_HELP,             {}, 0 },

  { "next",           DBGCMD_NEXT,             {}, 0 },
  { "step",           DBGCMD_STEP,             {}, 0 },

  { "x",              DBGCMD_X,                {}, 0 },

  { "quit",           DBGCMD_QUIT,             {}, 0 },

  { "_NOCMD",         DBGCMD_NOCMD,            {}, 0 }
};
internal struct dbg_cmd dbgCmd;

void
Dbg_Init();

void
Dbg_Prompt(char* cmdString);

bool
Dbg_ParseCmd(struct dbg_cmd* cmd, char* cmdString);

void
Dbg_ExecuteCmd(struct dbg_cmd* cmd);

int
Dbg_FindCmd(struct dbg_cmd* cmd, char* cmdString);

void
Dbg_DumpMemory(word_t address, u16 numUnits, char formatType, char unitType);

bool
Dbg_LoadProgram(char* path);

void
Dbg_PrintRegs();

void
Dbg_CmdNotImplemented(char* cmdString);


#define MEM_SIZE 256

internal bool    isRunning;
internal byte_t* memory;


int
main(int argc, char* argv[])
{
  char* debuggeePath;

  Log_Init();

  debuggeePath = 0;
  for (int argi = 1;
       argi < argc;
       ++argi)
  {
    if (argv[argi][0] != '-' &&
        !debuggeePath)
      debuggeePath = argv[argi];

    else if (strcmp(argv[argi], "--verbose-debug") == 0 ||
             strcmp(argv[argi], "-d") == 0)
      Log_SetVerbosity(3);
  }

  if (!debuggeePath)
  {
    PrintUsage(argv[0]);
    exit(-1);
  }

  Dbg_Init();

  CPU_Init(memory);
  memory = Mem_Init(MEM_SIZE);

  if (!Dbg_LoadProgram(debuggeePath))
  {
    ErrorFatal(ERRDBG_LOADPROGRAMFAILED);
  }

  isRunning = true;
  while (isRunning)
  {
    /* CPU_DoInstructionCycle(); */
    char cmdString[1024];

    Dbg_PrintRegs();
    Dbg_DumpMemory(0, 0x100, 'x', 'b');
    Dbg_Prompt(cmdString);
    Dbg_ParseCmd(&dbgCmd, cmdString);
    Dbg_ExecuteCmd(&dbgCmd);
  }

  return 0;
}

int
GetLine(char* line, u16 maxSize)
{
  int count;
  int c;

  count = 0;
  while ((c = getchar()) != EOF)
  {
    if (c == '\n')
      break;

    line[count] = c;
    if (++count == maxSize - 1) break;
  }

  line[count] = '\0';
  return count;
}

void
Dbg_Init()
{
  Dbg_FindCmd(&dbgCmd, "_NOCMD");
}

void
Dbg_Prompt(char* cmdString)
{
  const char* prompt = " > ";
  printf("[%04x]%s", CPU_GetProgramCounter(), prompt);
  GetLine(cmdString, 1024);
}

bool
Dbg_ParseCmd(struct dbg_cmd* cmd, char* cmdString)
{
  int  cmdFoundCount;
  char *cmdName;
  char *tok;
  char delims[] = " \t";
  u16  numParms;

  StripWhiteSpace(cmdString);

  /* tokenize cmdString */
  numParms = 0;
  tok = strtok(cmdString, delims);
  cmdName = tok;
  if (!cmdName)
    /* repeat previous cmd */
    return true;

  memset(cmd, 0, sizeof(struct dbg_cmd));

  /* ugly hack for 'x/FMT' syntax */
  if (CharInString(tok, '/'))
  {
    while (*tok != '/') tok++;
    *tok++ = '\0';
  }

  cmdFoundCount = Dbg_FindCmd(cmd, cmdName);
  if (cmdFoundCount == 0)
  {
#ifdef _DEBUG
    Log_Debug("Dbg_ParseCmd: cmdFoundCount == 0");
#endif
    cmd->cmdID = DBGCMD_NOCMD;
    fprintf(stderr, "Unknown command: %s\n", cmdString);
    return false;
  }
  else if (cmdFoundCount > 1)
  {
#ifdef _DEBUG
    Log_Debug("Dbg_ParseCmd: cmdFoundCount > 1 (%d)", cmdFoundCount);
#endif
    cmd->cmdID = DBGCMD_NOCMD;
    fprintf(stderr, "Ambiguous command: %s\n", cmdString);
    return false;
  }

  /* Handle cases where we already processed a '/' in the initial
     token (e.g. the 'x' command) */
  if (cmdName != tok)
  {
    cmd->flags |= DBGCMD_FLG_HASEXPARMS;
    if (strlen(tok) > 0)
    {
      strncpy(cmd->parms[numParms++], tok, MAX_PARM_LENGTH);
    }
    else
    {
      strncpy(cmd->parms[numParms++], "", MAX_PARM_LENGTH);
    }
  }

  tok = strtok(NULL, delims);
  while (tok && numParms < MAX_DBGCMD_PARMS)
  {
    /*
#ifdef _DEBUG
    Log_Debug("Dbg_ParseCmd: Adding parm:  %s", tok);
#endif
    */
    strncpy(cmd->parms[numParms++], tok, MAX_PARM_LENGTH);
    tok = strtok(NULL, delims);
  }
  if (numParms)
  {
    /* TODO: not sure if I'll even use this... */
    cmd->flags |= DBGCMD_FLG_HASPARMS;
  }
  
  return true;
}

void
Dbg_ExecuteCmd(struct dbg_cmd* cmd)
{

  /*
#ifdef _DEBUG
  Log_Debug("Dbg_ExecuteCmd: %s", cmd->cmdName); 
  Log_Debug("Dbg_ExecuteCmd: cmd->flags=0x%08x", cmd->flags);
  for (int i = 0;
        i < MAX_DBGCMD_PARMS;
        ++i)
  {
    Log_Debug("Dbg_ExecuteCmd [%s]: cmd->parms[%d] = \"%s\"", cmd->cmdName, i, cmd->parms[i]);
  }
#endif
  */

  switch (cmd->cmdID)
  {
  case DBGCMD_HELP:
    {
      Dbg_CmdNotImplemented("help");
      break;
    }

  case DBGCMD_NEXT:
    {
      u16 i, repeatCount; 


      repeatCount = atoi(cmd->parms[0]);
      if (repeatCount < 0)
      {
        fprintf(stderr, "next: invalid repeat count: %u\n", repeatCount);
        break;
      }
      if (repeatCount == 0) repeatCount = 1;

      /*
#ifdef _DEBUG
      Log_Debug("Dbg_ExecuteCmd [next]: repeatCount=%u", repeatCount);
#endif
      */
      
      for (i = 0;
           i < repeatCount;
           ++i)
      {
        CPU_DoInstructionCycle();
      }
      break;
    }

  case DBGCMD_X:
    {
      char*  fmt;
      u16    numUnits;
      char   formatType;
      char   unitType;
      word_t address;

      /* check for 'FMT' in 'x/FMT' */
      if (cmd->flags & DBGCMD_FLG_HASEXPARMS)
      {
#ifdef _DEBUG
        Log_Debug("Dbg_ExecuteCmd [x]: DBGCMD_FLG_HASEXPARMS");
#endif
        fmt = cmd->parms[0];
      }
      else
        fmt = 0;
      /*
#ifdef _DEBUG
      Log_Debug("Dbg_ExecuteCmd [x]: fmt=\"%s\"", fmt);
#endif
      */
      
#define DBGCMD_X_DEFAULT_NUMUNITS        1
#define DBGCMD_X_DEFAULT_FRMTTYPE      'x' 
#define DBGCMD_X_DEFAULT_UNITTYPE      'b'
      if (fmt) numUnits = atoi(fmt);
      else     numUnits = DBGCMD_X_DEFAULT_NUMUNITS;
      if (numUnits == 0)    numUnits = DBGCMD_X_DEFAULT_NUMUNITS;
      formatType = DBGCMD_X_DEFAULT_FRMTTYPE;
      unitType   = DBGCMD_X_DEFAULT_UNITTYPE;

      while (fmt && isdigit(*fmt)) ++fmt;
      if (fmt && *fmt)
      {
        formatType = *(fmt++);
      }
      if (fmt && *fmt)
      {
        unitType = *(fmt++);
      }

      if (cmd->flags & DBGCMD_FLG_HASEXPARMS)
        address = atoi(cmd->parms[1]);
      else
        address = atoi(cmd->parms[0]);

      /*
#ifdef _DEBUG
      Log_Debug("Dbg_ExecuteCmd [x]: address = 0x%04x", address);
#endif
      */

      Dbg_DumpMemory(address, numUnits, formatType, unitType);
      break;
    }

  case DBGCMD_QUIT:
    {
      isRunning = false;
      break;
    }

  case DBGCMD_NOCMD:
    {
      break;
    }

  default:
    {
      /* TODO: Sanity check? */
      return;
    }
  }
}

bool
Dbg_LoadProgram(char* path)
{
  FILE* fp;
  fp = fopen(path, "rb");
  if (!fp)
  {
    return false;
  }
  fread(memory, 1, MEM_SIZE, fp);
  fclose(fp);
  return true;
}

int
Dbg_FindCmd(struct dbg_cmd* cmd, char* cmdName)
{
  struct dbg_cmd* currCmd;
  int i;
  int foundCount;

  /*
#ifdef _DEBUG
  Log_Debug("Dbg_FindCmd: Searching for '%s'", cmdName);
#endif
  */

  foundCount = 0;
  for (i = 0;
       i < DBGCMD_NUMCMDS;
       ++i)
  {
    currCmd = &DebuggerCommands[i];
    /* TODO: Exact match should override multiple hits */
    if (strcmp(currCmd->cmdName, cmdName) == 0)
    {
      memcpy(cmd, currCmd, sizeof(struct dbg_cmd));
      /*
#ifdef _DEBUG
      Log_Debug("Dbg_FindCmd: Found '%s'", currCmd->cmdName); 
#endif
      */
      return 1;
    }
    else if (FuzzyCompare(currCmd->cmdName, cmdName))
    {
      memcpy(cmd, currCmd, sizeof(struct dbg_cmd));
      /*
#ifdef _DEBUG
      Log_Debug("Dbg_FindCmd: Found '%s'", currCmd->cmdName); 
#endif
      */
      ++foundCount;
    }
  }
  return foundCount;
}

void
Dbg_DumpMemory(word_t address, u16 numUnits, char formatType, char unitType)
{
  /*
    TODO: Implement all formats/unit types
    TODO: Allow for different numbering systems (hex/octal) as params
  */
  char* ValidFormatTypes = "acdfostux";
  char* ValidUnitTypes   = "bw";

  if (numUnits < 1) return;
  if (!CharInString(ValidFormatTypes, formatType))
  {
    fprintf(stderr, "Invalid format type: %c\n", formatType);
    return;
  }
  if (!CharInString(ValidUnitTypes, unitType))
  {
    fprintf(stderr, "Invalid unit type: %c\n", unitType);
    return;
  }


  char fmtString[10];
  switch (formatType)
  {
  case 'o':
    {
      u8 digits = (unitType == 'b') ? 4 : 8;
      sprintf(fmtString, " %%0%do", digits);
      break;
    }
  case 'x':
  default:
    {
      u8 digits = (unitType == 'b') ? 2 : 4;
      sprintf(fmtString, " %%0%dx", digits);
      break;
    }
  }
  printf("\n");
  u8  bytesPerRow = 16;
  u16 numCols = (unitType == 'w') ? 8 : 16;
  u16 numRows = (numUnits / numCols) + 1;
  u16 numBytes = numUnits * ((unitType == 'b') ? 1 : 2);
  u16 lastByte = address + numBytes - 1;
  u16 currByte = address;
  for (u16 row = 0;
       row < numRows && currByte <= lastByte;
       ++row)
  {
    printf("0x%04x: ", address + (bytesPerRow*row));
    for (u16 col = 0;
         col < numCols && currByte <= lastByte;
         ++col)
    {
      switch (unitType)
      {
      case 'b':
        {
          printf(fmtString, Mem_ReadByte(currByte));
          break;
        }
      case 'w':
        {
          printf(fmtString, Mem_ReadWord(currByte));
          break;
        }
      }
      currByte += (unitType == 'b') ? 1 : 2;
    }
    printf("\n");
  }
  printf("\n");
  
}

void
Dbg_CmdNotImplemented(char* cmdString)
{
  fprintf(stderr, "%s: not yet implemented\n", cmdString);
}

void
Dbg_PrintRegs()
{
  printf("\n");
  printf(" A: 0x%02x\n", CPU_GetRegValue(REG_A));
  printf(" B: 0x%02x\n", CPU_GetRegValue(REG_B));
  printf(" C: 0x%02x\n", CPU_GetRegValue(REG_C));
  printf(" D: 0x%02x\n", CPU_GetRegValue(REG_D));
  printf(" E: 0x%02x\n", CPU_GetRegValue(REG_E));
  printf(" H: 0x%02x\n", CPU_GetRegValue(REG_H));
  printf(" L: 0x%02x\n", CPU_GetRegValue(REG_L));

  printf("\n");
  printf("SP: 0x%04x\n", CPU_GetStackPointer());
  printf("PC: 0x%04x\n", CPU_GetProgramCounter());

  printf("\n");
  printf("Flags:");
  printf(" %s", CPU_GetFlag(FLG_SIGN)   ? "S"  : "s");
  printf(" %s", CPU_GetFlag(FLG_ZERO)   ? "Z"  : "z");
  printf(" %s", CPU_GetFlag(FLG_AUXCRY) ? "AC" : "ac");
  printf(" %s", CPU_GetFlag(FLG_PARITY) ? "P"  : "p");
  printf(" %s", CPU_GetFlag(FLG_CARRY)  ? "C"  : "c");
  printf("\n\n");
}
