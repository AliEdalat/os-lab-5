// W/R lock

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
#include "wr_lock.h"

void init_wr_lock(struct wr_lock* lk)
{
    sem_init(&lk->mutex,1);
    sem_init(&lk->okToRead,1);
    sem_init(&lk->okToWrite,1);

    lk ->ar = 0;
    lk ->aw = 0;
    lk ->ww = 0;
    lk ->wr = 0;
}

void acquire_writer_b(struct wr_lock* lk)
{   
    sem_wait(&lk->mutex);

    if (lk->aw + lk->ar > 0)
    {
        lk->ww+=1;
    } else {
        sem_signal(&lk->okToWrite);
        lk->aw += 1;
    }

    sem_signal(&lk->mutex);
    sem_wait(&lk->okToWrite);
}

void release_writer_b(struct wr_lock* lk){
    sem_wait(&lk->mutex);
    lk->aw -= 1;
    if (lk->ww > 0){
        sem_signal(&lk->okToWrite);
        lk->aw -= 1;
        lk->ww -= 1;
    } else if(lk->wr > 0){
         sem_signal(&lk->okToRead);
         lk->ar += 1;
         lk->wr -= 1;
    }
    sem_signal(&lk->mutex);
}

void acquire_reader_b(struct wr_lock* lk)
{
    sem_wait(&lk->mutex);                   
    if (lk->aw + lk->ww > 0)                   
    {
        lk->wr += 1; 
    } else {
        sem_signal(&lk->okToRead);
        lk->ar += 1;
    }      
    sem_signal(&lk->mutex);                   
    sem_wait(&lk->okToRead);
}

void release_reader_b(struct wr_lock* lk)
{
    sem_wait(&lk->mutex);                    
    lk->ar-=1;                     
    if (lk->ar == 0 && lk->ww > 0)
    {                
        lk->ww -= 1;  
        lk->aw += 1;
    }              
    sem_signal(&lk->mutex); 
}