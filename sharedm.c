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
#define MAXSHM         10 // maximum amount of shared memory blocks in OS
#define PROCNUM        64 // maximum number of processes

struct shmblock
{
    int id;
    int owner;
    int flags;
    int ref_count;
    int size;
    char* pages[MAXSHMPBLOCK];
    int members[PROCNUM];
};

struct {
    struct spinlock shmlock;
    struct shmblock blocks[MAXSHM];
} shmtable;

int start = 1;

void shminit(void)
{
    int i,j;
    initlock(&shmtable.shmlock,"shmtable");
    for(i = 0; i < MAXSHM; i++) {
	shmtable.blocks[i].size = 0;
  	shmtable.blocks[i].ref_count = 0;
    for(j = 0; j < MAXSHMPBLOCK ; j++)
        shmtable.blocks[i].pages[j] = 0;
    for(j = 0; j < PROCNUM ; j++)
        shmtable.blocks[i].members[j] = 0;
    }
    // cprintf("dd\n");
}

int shmopen(int id, int page_count, int flag)
{
    int i, j;
    if(start){
        shminit();
        start=0;
    }

    for(i = 0; i < MAXSHM; i++)
        if (shmtable.blocks[i].id == id)
            break;

    if (i == MAXSHM) {
        acquire(&shmtable.shmlock);
        for(i = 0; i < MAXSHM; i++) {
            if (shmtable.blocks[i].ref_count == 0) {
                // cprintf("dud\n");// TODO : check page_count <= MAXSHMPBLOCK TEST
                
                if (page_count > MAXSHMPBLOCK)
                    panic("Number of pages is larger than available blocks!:D");
                
                for(j = 0; j < page_count; j++) {
                    shmtable.blocks[i].pages[j] = (char*)kalloc();
                }

		        // cprintf("dod\n");
                shmtable.blocks[i].id = id;
                shmtable.blocks[i].owner = myproc()->pid;
                shmtable.blocks[i].flags = flag;
                shmtable.blocks[i].ref_count = 1;
                shmtable.blocks[i].size = page_count;
                release(&shmtable.shmlock);
                return 0;
            }  
        }
        release(&shmtable.shmlock);
        cprintf("shared mem is full!!");
        return -3;
    } else {
        //release(&shmtable.shmlock);
        cprintf("Shared mem is already open!!");
        return -1;
    }
}

void* shmattach(int id) {
    int i, k, start_va = 0;
    for(i = 0; i < MAXSHM; i++) {
        if (shmtable.blocks[i].id == id) {
            if(shmtable.blocks[i].flags == 0) {
                if (myproc()->pid == shmtable.blocks[i].owner)
                {
                    acquire(&shmtable.shmlock);
                    if ((start_va = shm_allocuvm(myproc()->pgdir, shmtable.blocks[i].pages, shmtable.blocks[i].size,
                            PTE_P|PTE_W|PTE_U)) == 0)
                    {
                        cprintf("did\n");
                        release(&shmtable.shmlock);
                        panic("bad shared mem allocation!!");
                    }
                    release(&shmtable.shmlock);
                } else {
                    acquire(&shmtable.shmlock);
                    if ((start_va = shm_allocuvm(myproc()->pgdir, shmtable.blocks[i].pages, shmtable.blocks[i].size,
                            PTE_P|PTE_U)) == 0)
                    {
                        cprintf("did\n");
                        release(&shmtable.shmlock);
                        panic("bad shared mem allocation!!");
                    }
                    release(&shmtable.shmlock);
                }
            } else {
                if (myproc()->pid == shmtable.blocks[i].owner || myproc()->parent->pid == shmtable.blocks[i].owner)
                {
                    acquire(&shmtable.shmlock);
                    if ((start_va = shm_allocuvm(myproc()->pgdir, shmtable.blocks[i].pages, shmtable.blocks[i].size,
                            PTE_P|PTE_W|PTE_U)) == 0)
                    {
                        cprintf("did\n");
                        release(&shmtable.shmlock);
                        panic("bad shared mem allocation!!");
                    }
                    release(&shmtable.shmlock);
                }
                else{
                    release(&shmtable.shmlock);
                    panic("Access denied!!");
                }
            }
            acquire(&shmtable.shmlock);
            shmtable.blocks[i].members[shmtable.blocks[i].ref_count-1] = myproc()->pid;
            shmtable.blocks[i].ref_count++;
            for( k = 0; k < shmtable.blocks[i].size; k++)
            {
                myproc()->shmPages[myproc()->index] = (start_va+(k*PGSIZE));
                myproc()->paPages[myproc()->index] = /*(int)(shmtable.blocks[i].pages[k]);*/(int)walkpgdir(myproc()->pgdir,(char*)start_va+(k*PGSIZE),0);
                myproc()->index++;
                cprintf("pid : %d, va : %p, pa : %p\n",myproc()->pid, (char*)start_va+(k*PGSIZE), (char*)walkpgdir(myproc()->pgdir,(char*)start_va+(k*PGSIZE),0));
            }
            
            for( k = 0; k < myproc()->index; k++){
                cprintf("v:%p, p: %p\n",(char*)myproc()->shmPages[k],(char*)myproc()->paPages[k]);
            }

            // myproc()->shmPages[myproc()->index++] = (uint)start_va;
            release(&shmtable.shmlock);
            return (char*)start_va;
        }
    } 
    return 0;  
}

int shmclose(int id)
{ 
    int i, j, k;
    for(i = 0; i < MAXSHM; i++) {
        if (shmtable.blocks[i].id == id) {
            acquire(&shmtable.shmlock);
            for(j = 0; j < PROCNUM ; j++){
                if (myproc()->pid == shmtable.blocks[i].owner || myproc()->pid == shmtable.blocks[i].members[j]){
                    shmtable.blocks[i].ref_count--;
                    if (shmtable.blocks[i].ref_count == 0)
                    {
                        for (k = 0; k < shmtable.blocks[i].size; ++k)
                        {
                            kfree(shmtable.blocks[i].pages[k]);
                        }
                    }
                    break;
                }
            }
            release(&shmtable.shmlock);
        }
    }
    return 0;
}
