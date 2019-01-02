// #include "semaphore.h"

struct rw_lock{
    struct semaphore mutex, wrt;
    int readcount;
};  