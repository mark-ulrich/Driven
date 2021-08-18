#include <stdio.h>
#include <string.h>


int main()
{
  char delims[]   = "/ \t";
  char test_str[] = "x/16x   0x10  bel       adk";
  char *tok;
  unsigned short tokCount;

#define MAX_TOKENS 4
  tokCount = 0;
  tok = strtok(test_str, delims);
  while (tok)
  {
    if(++tokCount > MAX_TOKENS)
      break;

    printf("%s\n", tok);
    tok = strtok(NULL, delims);
  }

  return 0;
}
