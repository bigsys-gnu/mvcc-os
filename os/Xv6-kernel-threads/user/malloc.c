#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

#define KB 1000000

int
main(int argc, char *argv[])
{
	int i;
	for (i = 0; i < KB * 1000; i += 1000)
	  {
		void *tmp = malloc(i);
		printf(1, "allocated: %d\n", i);
		free(tmp);
	  }

  	exit();
}
