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
#include "ticket_lock.h"
#include "rwt_lock.h"

void init_rwt_lock(struct rwt_lock* lk)
{
    init_ticket_lock(&lk->mutex,"mutex");
    init_ticket_lock(&lk->wrt,"wrt");
    lk ->readcount = 0;
}

void acquire_writer_tt(struct rwt_lock* lk)
{
    ticket_acquire(&lk->wrt);
}

void release_writer_tt(struct rwt_lock* lk){
    ticket_release(&lk->wrt);
}

void acquire_reader_tt(struct rwt_lock* lk)
{
    ticket_acquire(&lk->mutex);
    lk->readcount++;
    if (lk->readcount == 1)
        ticket_acquire(&lk->wrt);
    ticket_release(&lk->mutex);
}

void release_reader_tt(struct rwt_lock* lk)
{
    ticket_acquire(&lk->mutex);
    lk->readcount--;
    if(lk->readcount == 0)
        ticket_release(&lk->wrt);
    ticket_release(&lk->mutex);
}