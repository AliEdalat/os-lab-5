#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
	char* a;
	shm_open(1,3,0);
	a = shm_attach(1);
	*a = 'r';
	exit();
}
