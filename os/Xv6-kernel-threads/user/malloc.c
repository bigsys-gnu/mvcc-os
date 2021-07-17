#include "user.h"

#define KB 1000000

int
main(int argc, char *argv[])
{
<<<<<<< HEAD
	int i;
	for (i = 0; i < KB * 1000; i += 1000)
	  {
		void *tmp = malloc(i);
		printf(1, "allocated: %d\n", i);
		free(tmp);
	  }

  	exit();
=======
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
>>>>>>> origin/user-level-rlu
}
