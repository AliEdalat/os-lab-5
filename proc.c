#include "types.h"
#include "defs.h"
#include "param.h"
#include "date.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
int cdate = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  int i;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->ctime = ticks;
  p->priority = 10;
  p->MFQpriority = 1;
  p->tickets = 100;
  for (i = 0; i < 41; ++i)
  {
    p->syscalls[i].count = 0;
  }

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->ctime = 0;
        p->state = UNUSED;
        p->priority = 0;
        p->MFQpriority = 0;
        p->tickets = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

void
MFQscheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  int MFQpriority = 1;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    struct proc *minP = 0;
    struct proc *highP = 0;
    int rand = -1;
    int found = 0;
    acquire(&ptable.lock);
    for (int i = 1; i <= MFQpriority; ++i)
    {
	    if (i == 1){
		    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		        if(p->state != RUNNABLE || p->MFQpriority != i)
		            continue;
	            int total = totalTickets();
	            if (total > 0 && rand <= 0){
	            	rand = random(total);
	            }

	            rand -= p->tickets;
	            if(rand <= 0){
	                p = p;
	                found = 1;
	            }
            	if(p != 0 && found == 1)
				{
			    	c->proc = p;
			    	switchuvm(p);
			    	p->state = RUNNING;

			    	swtch(&(c->scheduler), p->context);
			    	switchkvm();

			    	c->proc = 0;
			    	break;
				}
	        }
	    }
	    else if (i == 2)
    	{
    		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		        if(p->state != RUNNABLE || p->MFQpriority != i)
		            continue;
	            if (minP != 0){
	                if(p->ctime < minP->ctime){
	                    minP = p;
                    }
	            }
	            else{
	                minP = p;
                }
	        }

            if(minP != 0 && minP->state == RUNNABLE){
                p = minP;
                found = 1;
            }
            if(p != 0 && found == 1)
			{
		    	c->proc = p;
		    	switchuvm(p);
		    	p->state = RUNNING;

		    	swtch(&(c->scheduler), p->context);
		    	switchkvm();

		    	c->proc = 0;
			}
    	} else if (i == 3) {
    		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		        if(p->state != RUNNABLE || p->MFQpriority != i)
		            continue;
	    		if (highP == 0)
	    		{
	    			highP = p;
	    		}
	            if(highP->priority >= p->priority)
	                highP = p;
	        }

	    	if(highP != 0){
                p = highP;
                found = 1;
	    	}
	    	if(p != 0 && found == 1)
			{
		    	c->proc = p;
		    	switchuvm(p);
		    	p->state = RUNNING;

		    	swtch(&(c->scheduler), p->context);
		    	switchkvm();

		    	c->proc = 0;
			}
    	}
	    if (found == 1)
	    {
	    	MFQpriority = i;
	    	break;
	    }
	}

    if (found == 0 )
    {
    	if (MFQpriority < 3)
            MFQpriority++;
        else
            MFQpriority = 1;
    }
    release(&ptable.lock);
  }
}

int
random(int max) {

  if(max <= 0) {
    return 1;
  }

  static int z1 = 12345; // 12345 for rest of zx
  static int z2 = 12345; // 12345 for rest of zx
  static int z3 = 12345; // 12345 for rest of zx
  static int z4 = 12345; // 12345 for rest of zx

  int b;
  b = (((z1 << 6) ^ z1) >> 13);
  z1 = (((z1 & 4294967294) << 18) ^ b);
  b = (((z2 << 2) ^ z2) >> 27);
  z2 = (((z2 & 4294967288) << 2) ^ b);
  b = (((z3 << 13) ^ z3) >> 21);
  z3 = (((z3 & 4294967280) << 7) ^ b);
  b = (((z4 << 3) ^ z4) >> 12);
  z4 = (((z4 & 4294967168) << 13) ^ b);

  // if we have an argument, then we can use it
  int rand = ((z1 ^ z2 ^ z3 ^ z4)) % max;

  if(rand < 0) {
    rand = rand * -1;
  }

  return rand;
}

int
totalTickets(void) {

	struct proc *p;
	int total = 0;
	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
		if (p->state == RUNNABLE && p->MFQpriority == 1) {
			total += p->tickets;
		}
	}
	return total;
}

void
change_tickets(int pid, int tickets)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid) {
        p->tickets = tickets;
        break;
    }
  }
  release(&ptable.lock);
}

void
ps(void)
{
  struct proc *p;

  acquire(&ptable.lock);
  cprintf("NAME\tPID\tSTATE\t\tPRIORITY\tTICKETS\tCTIME\tLEVEL\n");
  cprintf("---------------------------------------------------------------------------------------------------\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
	if(p->state == UNUSED)
		continue;
  	cprintf("%s", p->name);
   	cprintf("\t%d", p->pid);
   	switch(p->state){
  	case UNUSED:
  		cprintf("\t%s", "UNUSED  ");
  		break;
  	case EMBRYO:
  		cprintf("\t%s", "EMBRYO  ");
  		break;
  	case SLEEPING:
  		cprintf("\t%s", "SLEEPING");
  		break;
  	case RUNNABLE:
  		cprintf("\t%s", "RUNNABLE");
  		break;
  	case RUNNING:
  		cprintf("\t%s", "RUNNING ");
  		break;
  	case ZOMBIE:
  		cprintf("\t%s", "ZOMBIE  ");
  		break;
  	}

    cprintf("\t%d", p->priority);
    cprintf("\t\t%d", p->tickets);
    cprintf("\t%d", p->ctime);
    cprintf("\t%d\n\n", p->MFQpriority);
  }
  release(&ptable.lock);
}

// Change Process priority
void
chpr(int pid, int priority)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid) {
        p->priority = priority;
        break;
    }
  }
  release(&ptable.lock);
}

// Change Process priority
void
chmfq(int pid, int priority)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid) {
        p->MFQpriority = priority;
        break;
    }
  }
  release(&ptable.lock);
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

void
sleep_without_spin(void *chan)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");


  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  
  acquire(&ptable.lock);  //DOC: sleeplock1
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&ptable.lock);
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

int
invocation_log(int pid)
{
  struct proc *p;
  int i, status = -1;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->pid == pid){
      for (i = 0; i < 41; ++i)
      {
        if (p->syscalls[i].count > 0)
        {
          struct date* d = p->syscalls[i].datelist;
          struct syscallarg* a = p->syscalls[i].arglist;
          for (; d != 0 && a != 0; d = d->next)
          {
            cprintf("%d syscall : ID :%d NAME:%s DATE: %d:%d:%d %d-%d-%d\n",p->syscalls[i].count, i+1,
              p->syscalls[i].name, d->date.hour, d->date.minute, d->date.second, d->date.year,
              d->date.month, d->date.day);
            if (i == 0 || i == 1 || i == 2 || i == 13 || i == 10 || i == 27 || i == 28)
              cprintf("%d %s  (%s)\n",p->pid, p->syscalls[i].name, a->type[0]); 
            if (i == 21 || i == 22 || i == 24 || i == 5 || i == 11 || i == 12 || i == 9 || i == 20 || i == 39 || i == 40)
              cprintf("%d %s  (%s %d)\n",p->pid, p->syscalls[i].name, a->type[0], a->int_argv[0]);
            if (i == 23 || i == 33 || i == 34)
              cprintf("%d %s  (%s %d, %s %d)\n",p->pid, p->syscalls[i].name,
                a->type[0],a->int_argv[0],
                a->type[1],a->int_argv[1]);
            if (i == 3)
              cprintf("%d %s  (%s 0x%p)\n",p->pid, p->syscalls[i].name, a->type[0], a->intptr_argv);
            if (i == 4 || i == 15)
              cprintf("%d %s  (%s %d, %s 0x%p, %s %d)\n",p->pid, p->syscalls[i].name,
                a->type[0],a->int_argv[0],a->type[1],a->str_argv[0],
                a->type[2],a->int_argv[1]);
            if (i == 6)
                cprintf("%d %s  (%s 0x%p, %s 0x%p)\n",p->pid, p->syscalls[i].name, a->type[0], a->str_argv[0], a->type[1], a->ptr_argv[0]);
            if (i == 14)
                cprintf("%d %s  (%s %s, %s %d)\n",p->pid, p->syscalls[i].name, a->type[0], a->str_argv[0], a->type[1], a->int_argv[0]);
            if (i == 17 || i == 19 || i == 8)
                cprintf("%d %s  (%s %s)\n",p->pid, p->syscalls[i].name, a->type[0], a->str_argv[0]);
            if (i == 18)
                cprintf("%d %s  (%s %s, %s %s)\n",p->pid, p->syscalls[i].name, a->type[0], a->str_argv[0], a->type[1], a->str_argv[1]);
            if (i == 7)
                cprintf("%d %s  (%s %d, %s 0x%p)\n",p->pid, p->syscalls[i].name, a->type[0], a->int_argv[0], a->type[1], a->st);
            if (i == 16)
                cprintf("%d %s  (%s %s, %s %d, %s %d)\n",p->pid, p->syscalls[i].name, a->type[0], a->str_argv[0], a->type[1], a->int_argv[0],
                  a->type[2], a->int_argv[1]);
            if (i == 38)
                cprintf("%d %s  (%s %d, %s %d, %s %d)\n",p->pid, p->syscalls[i].name, a->type[0], a->int_argv[0], a->type[1], a->int_argv[1],
                  a->type[2], a->int_argv[2]);

            a = a->next;
          }
          status = 0;
        } 
      }
    }
  }

  release(&ptable.lock);

  if (status == -1)
    cprintf("pid not found!\n");

  return status;
}

int
get_syscall_count(int pid, int sysnum)
{
  struct proc *p;
  int count = 0, status = -1;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
     if (p->pid == pid)
     {
       count = p->syscalls[sysnum-1].count;
       status = 0;
     }
  }

  if(status == -1)
  {
    cprintf("pid not found!\n");
    release(&ptable.lock);
    return -1;
  }
  release(&ptable.lock);
  return count;
}

void 
log_syscalls(struct node* first_proc)
{
  for(struct node* n = first_proc; n != 0; n = n->next){
    cprintf("Syscall name: %s @ DATE: %d:%d:%d %d-%d-%d by Process: %d\n", n -> name, n->date.hour, n->date.minute, n->date.second, n->date.year,
      n->date.month, n->date.day, n -> pid);
  }
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  int count=0;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    for(i=0;i<24;i++)
      count += p->syscalls[i].count;
    cprintf("%d %s %s count:%d", p->pid, state, p->name,count);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
