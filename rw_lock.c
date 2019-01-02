// R/W lock

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "date.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "semaphore.h"
#include "rw_lock.h"

void init_rw_lock(struct rw_lock* lk)
{
    sem_init(&lk->mutex,1);
    sem_init(&lk->wrt,1);
    lk ->readcount = 0;
}

void acquire_writer(struct rw_lock* lk)
{
    sem_wait(&lk->wrt);
}

void release_writer(struct rw_lock* lk){
    sem_signal(&lk->wrt);
}

void acquire_reader(struct rw_lock* lk)
{
    sem_wait(&lk->mutex);
    lk->readcount++;
    if (lk->readcount == 1)
        sem_wait(&lk->wrt);
    sem_signal(&lk->mutex);
}

void release_reader(struct rw_lock* lk)
{
    sem_wait(&lk->mutex);
    lk->readcount--;
    if(lk->readcount == 0)
        sem_signal(&lk->wrt);
    sem_signal(&lk->mutex);
}