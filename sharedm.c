#include "types.h"
#include "defs.h"
#include "param.h"
#include "date.h"
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

int start = 1;

void shminit(void)
{
    int i,j;
    initlock(&shmtable.shmlock,"shmtable");
    cprintf("dd\n");
    for(i = 0; i < MAXSHM; i++) {
	shmtable.blocks[i].size = 0;
  	shmtable.blocks[i].ref_count = -1;
        for(j = 0; j < MAXSHMPBLOCK ; j++)
           shmtable.blocks[i].pages[j] = 0;
    }
    cprintf("dd\n");
}

int shmopen(int id, int page_count, int flag)
{
    int i, j;
    //acquire(&shmtable.shmlock);
    if(start){
        shminit();
        start=0;
    }
    for(i = 0; i < MAXSHM; i++) {
        if (shmtable.blocks[i].id == id) {
            break;
        }  
    }

    if (i == MAXSHM) {
        for(i = 0; i < MAXSHM; i++) {
            if (shmtable.blocks[i].ref_count == -1) {
                cprintf("dud\n");// TODO : check page_count <= MAXSHMPBLOCK
                acquire(&shmtable.shmlock);
                for(j = 0; j < page_count; j++) {
                    shmtable.blocks[i].pages[j] = (char*)kalloc();
                }
		        cprintf("dod\n");
                shmtable.blocks[i].id = id;
                shmtable.blocks[i].owner = myproc()->pid;
                shmtable.blocks[i].flags = flag;
                shmtable.blocks[i].ref_count = 0;
                shmtable.blocks[i].size = page_count;
                release(&shmtable.shmlock);
                return 0;
            }  
        }
        //release(&shmtable.shmlock);
        cprintf("shared mem is full!!");
        return -3;
    } else {
        //release(&shmtable.shmlock);
        cprintf("reopen shared mem!!");
        return -1;
    }
}

void* shmattach(int id)
{
    int i, start_va = 0;
    for(i = 0; i < MAXSHM; i++) {
        if (shmtable.blocks[i].id == id) {
            if(shmtable.blocks[i].flags == 0) {
                if (myproc()->pid == shmtable.blocks[i].owner)
                {
                    acquire(&shmtable.shmlock);
                    if ((start_va = shm_allocuvm(myproc()->pgdir, shmtable.blocks[i].pages, shmtable.blocks[i].size,
                            PTE_W|PTE_U)) == 0)
                    {
                        cprintf("did\n");
                        release(&shmtable.shmlock);
                        panic("bad shared mem allocation!!");
                    }
                    release(&shmtable.shmlock);
                } else {
                    acquire(&shmtable.shmlock);
                    if ((start_va = shm_allocuvm(myproc()->pgdir, shmtable.blocks[i].pages, shmtable.blocks[i].size,
                            PTE_U)) == 0)
                    {
                        cprintf("did\n");
                        release(&shmtable.shmlock);
                        panic("bad shared mem allocation!!");
                    }
                    release(&shmtable.shmlock);
                }
            } else {

            }
            shmtable.blocks[i].ref_count++;
            return (char*)start_va;
        }  
    } 
    return 0;  
}

int shmclose(int id)
{
    return 0;
}
