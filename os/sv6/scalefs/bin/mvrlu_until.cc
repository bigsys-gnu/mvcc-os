#include <stdio.h>
#include <stdlib.h>
#include "user.h"

int main(int argc, char *argv[])
{
  unsigned int until = atoi(argv[1]);
  if (until >= 0)
  {
    mvrlu_until(until);
    printf("%d new value\n", until);
  }
  return 0;
}
