#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
	char* a;
	shm_open(1,3,0);
	fork();
	a = shm_attach(1);
	a[0] = 'r';
	a[1] = 'u';
	a[2] = 'c';
	a[3] = '\0';
	printf(1,">>%s\n", a);
	wait();
	shm_close(1);
	exit();
}
