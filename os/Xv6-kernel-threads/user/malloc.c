#include "user.h"

int
main(int argc, char *argv[])
{
  void *test;
  int i;

  for (i = 0; i < 4001; ++i)
	{
	  if (!(test = malloc(i)))
		{
		  printf(1, "%p\n", test);
		  free(test);
		}
	  printf(1, "%d byte is maximum.\n", i);
	}
  
  exit();
}
