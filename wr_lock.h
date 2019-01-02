// #include "semaphore.h"

struct wr_lock{
    struct semaphore mutex, okToRead, okToWrite;
    int aw, ww, ar, wr; 
};  