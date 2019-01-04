#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
	shm_open(1,3,1);
	//printf(1,"pp\n");
	exit();
}
