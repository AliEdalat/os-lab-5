#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "syscall.h"

int
main(int argc, char* argv[])
{
    chmfq(atoi(argv[1]), atoi(argv[2]));
    exit();
}
