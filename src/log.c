#include "types.h"
#include "log.h"

#include <stdarg.h>
#include <stdio.h>

internal FILE* dbgStream;
internal u8    logVerbosity;

void
Log_Init()
{
  dbgStream    = stderr;
  logVerbosity = 0;
}

void
Log_Debug(char* fmt, ...)
{
  if (logVerbosity < 3) return;
  va_list args;
  va_start(args, fmt);
  fprintf(dbgStream, "DEBUG: ");
  vfprintf(dbgStream, fmt, args);
  fprintf(dbgStream, "\n");
  va_end(args);
}

void
Log_SetVerbosity(u8 verbosity)
{
  logVerbosity = verbosity;
}
