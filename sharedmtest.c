#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
	char* a;
	char* b;
	int pid;
	shm_open(1,3,1);
	b = shm_attach(1);
	if ((pid = fork()) == 0){
		// a = shm_attach(1);
		// printf(1,"b pointer in child : %p\n",b);
		b[0] = 'r';
		b[1] = 'u';
		b[2] = 'c';
		b[3] = 'a';
		b[4] = '\0';
		printf(1,"child pid : %d\n",getpid());
		// printf(1,">>%s\n", b);
		a = a;
	}
	else{
		sleep(100);
		// a = shm_attach(1);
		printf(1,"parent pid : %d\n",getpid());
		// printf(1,"b pointer in parent : %p\n",b);
		printf(1,">>%s\n", b);
		// shm_close(1);
	}
	sleep(500);
	exit();
}
