#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

#define MAXSHMPBLOCK   4  // maximum amount of pages in shared memory block
#define MAXSHM         10 // maximun amount of shared memory blocks in OS

struct shmblock
{
    int id;
    int owner;
    int flags;
    int ref_count;
    int size;
    char* pages[MAXSHMPBLOCK];
};

struct {
    struct spinlock shmlock;
    struct shmblock blocks[MAXSHM];
}shmtable;

void shm_init(void)
{
  int i,j;

  initlock(&shmtable.shmlock,"shmtable");
  
    for(i = 0; i < MAXSHM; i++) {
  	    shmtable.blocks[i].size = 0;
  	    shmtable.blocks[i].ref_count = -1;
        for(j = 0; j < MAXSHMPBLOCK ; j++)
            shmtable.blocks[i].pages[j] = 0;
    }
}

int sys_shm_open(int id, int page_count, int flag)
{
    int i;
    for(i = 0; i < MAXSHM; i++) {
        if (shmtable.blocks[i].id == id) {
            break;
        }  
    }

    if (i == MAXSHM) {
        for(i = 0; i < MAXSHM; i++) {
            if (shmtable.blocks[i].ref_count == -1) {
                
                shmtable.blocks[i].id = id;
                shmtable.blocks[i].owner = myproc()->pid;
                shmtable.blocks[i].flags = flag;
                shmtable.blocks[i].ref_count = 0;
                shmtable.blocks[i].size = page_count;
            }  
        }
    } else {
        panic("reopen shared mem!!");
        return -1;
    }
}

void* sys_shm_attach(int id)
{

}

int sys_shm_close(int id)
{

}
