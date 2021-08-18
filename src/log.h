#ifndef __LOG_H__
#define __LOG_H__

#include "types.h"


void
Log_Init();

void
Log_Debug(char* fmt, ...);

void
Log_SetVerbosity(u8 verbosity);

#endif    /* __LOG_H__ */
