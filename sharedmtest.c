#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
	char* a;
	shm_open(1,3,1);
	fork();
	a = shm_attach(1);
	*a = 'r';
	printf(1,">>%c\n", *a);
	wait();
	shm_close(1);
	exit();
}
