#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

//same lock for everything
struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc *proc_queue[5][NPROC];
  int tail[5];
  int head[5];
} ptable;

int ticksforqq[5] = {1, 2, 4, 8, 16};
//MLFQ FUNCTIONS

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
  for (int i = 0; i < 5; i++)
    ptable.head[i] = ptable.tail[i] = 0;
}

void add_to_queue(int queue, struct proc *p)
{
  for (int i=ptable.head[queue];i!=ptable.tail[queue];i = (i+1)%NPROC)
  {
    if (ptable.proc_queue[queue][i]->pid==p->pid)
      return;
  }
  ptable.proc_queue[queue][ptable.tail[queue]] = p;
  ptable.tail[queue] = (ptable.tail[queue] + 1) % NPROC;
  // cprintf("Adding proc with pid %d to queue %d\n",p->pid,queue);
}

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void *chan);

int cpuid()
{
  return mycpu() - cpus;
}
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->queue = 0;

  release(&ptable.lock);

  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  acquire(&tickslock);
  p->ctime = ticks;
  p->lcheck = ticks;
  release(&tickslock);
  p->etime = 0;
  p->rtime = 0;
  p->iotime = 0;
  p->priority = 60;
  for (int i = 0; i < 5; i++)
    p->ticksinq[i] = 0;
  return p;
}

void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
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
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  acquire(&ptable.lock);
#ifdef MLFQ
  add_to_queue(0, p);
#endif
  p->state = RUNNABLE;
  release(&ptable.lock);
}

int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

#ifdef MLFQ
  add_to_queue(0, np);
#endif
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;
  acquire(&ptable.lock);

  wakeup1(curproc->parent);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
      {
        wakeup1(initproc);
      }
    }
  }
  curproc->state = ZOMBIE;
  acquire(&tickslock);
  curproc->etime = ticks;
  release(&tickslock);
  sched();
  panic("zombie exit");
}

int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->queue = -1;

        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}

int waitx(int *wtime, int *rtime)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        pid = p->pid;
        *wtime = p->etime - p->ctime - p->rtime - p->iotime;
        *rtime = p->rtime;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        p->etime = 0;
        p->ctime = 0;
        p->rtime = 0;
        p->iotime = 0;
        p->queue = -1;
        release(&ptable.lock);
        return pid;
      }
    }

    if (!havekids || curproc->killed)
    {
      *rtime = 0, *wtime = 0;
      release(&ptable.lock);
      return -1;
    }

    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}

int set_priority(int priority)
{
  int yield_flag = 0;
  struct proc *p = myproc();
  acquire(&ptable.lock);
  int ret = p->priority;
  p->priority = priority;
  if (ret > p->priority)
    yield_flag = 1;
  release(&ptable.lock);
  if (yield_flag)
  {
    yield();
  }
  return ret;
}

char *val(int state)
{
  switch (state)
  {
  case 0:
    return "UNUSED";
  case 1:
    return "EMBRYO";
  case 2:
    return "SLEEPING";
  case 3:
    return "RUNNABLE";
  case 4:
    return "RUNNING";
  case 5:
    return "ZOMBIE";
  }
  return "ERROR";
}

int ps(void)
{
  struct proc *p;
  cprintf("\n\nPID     NAME     STATE     PRIORITY \n");
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid != 0)
      ;
    cprintf(" %d     %s     %s     %d\n", p->pid, p->name, val(p->state), p->priority);
  }
  cprintf("\n\n");

  return 1;
}

void scheduler(void)
{
  struct proc *p;
  struct cpu *cpu = mycpu();
  cpu->proc = 0;
  for (;;)
  {
    sti();

    acquire(&ptable.lock);
#ifdef DEFAULT
    // cprintf("DEFAULT RR\n");
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
        continue;
      cpu->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, p->context);
      switchkvm();
      cpu->proc = 0;
    }

#else

#ifdef FCFS
    // cprintf("FCFS\n");
    struct proc *min = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      // select the proc with min creation time
      if (p->state == RUNNABLE)
      {
        if (min != 0)
        {
          if (p->ctime < min->ctime)
            min = p;
        }
        else
          min = p;
      }
    }
    //run the process with min creation time
    if (min != 0)
    {
      p = min; //the process with the smallest creation time
      cpu->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, p->context);
      switchkvm();
      cpu->proc = 0;
    }
#else
#ifdef PBS
    int max_priority = 500;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      // select the max priority
      if (p->state == RUNNABLE)
      {
        max_priority = max_priority < p->priority ? max_priority : p->priority;
      }
    }
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE || p->priority != max_priority)
        continue;

      // cprintf("max_priority chosen is %d with pid %d\n", max_priority, p->pid);
      cpu->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, p->context);
      switchkvm();
      if (p->state == RUNNABLE)
      {
        //   //proccess exited due to change in priority
        //   //rerun the scheduler
        break;
      }
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      cpu->proc = 0;
    }
#else
#ifdef MLFQ
    int curqueue = 0;
    // cprintf("MLFQ\n");
    for (; curqueue < 5; curqueue++)
    {

      // cprintf("head %d tail %d for queue %d\n",ptable.head[curqueue],ptable.tail[curqueue],curqueue);
      for (int i = ptable.head[curqueue]; i != ptable.tail[curqueue]; i = (i + 1) % NPROC)
      {
        p = ptable.proc_queue[curqueue][i];
        ptable.head[curqueue] = (ptable.head[curqueue] + 1) % NPROC;
        // cprintf("HRERE %d\n",p->pid);
        // cprintf("%d\n",p->queue);
        if (p==0 || p->queue != curqueue || p->state != RUNNABLE)
          continue;
        // cprintf("Running proc with pid %d\n",p->pid);
        p->num_run++;
        p->choosetime=ticks;
        cprintf("Process with pid %d and state %d chosen at time %d from queue %d at index %d\n",p->pid,p->state,p->choosetime,p->queue,i);
        cpu->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        swtch(&(cpu->scheduler), p->context);
        switchkvm();
        cpu->proc = 0;
        curqueue = 0;
      }
    }
#endif
#endif
#endif
#endif
    release(&ptable.lock);
  }
}

void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

void yield(void)
{
  acquire(&ptable.lock); //DOC: yieldlock
  struct proc *p = myproc();
  p->state = RUNNABLE;
#ifdef MLFQ
  if (p->queue != 4)
    p->queue++;
  cprintf("Demoting process with pid %d to queue %d\n",p->pid,p->queue);
  add_to_queue(p->queue, p);
#endif
  sched();
  release(&ptable.lock);
}

void forkret(void)
{
  static int first = 1;
  release(&ptable.lock);

  if (first)
  {
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
}

void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  if (lk != &ptable.lock)
  {                        //DOC: sleeplock0
    acquire(&ptable.lock); //DOC: sleeplock1
    release(lk);
  }
  p->chan = chan;
  p->state = SLEEPING;
  sched();
  p->chan = 0;
  if (lk != &ptable.lock)
  { //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
    {
#ifdef MLFQ
      add_to_queue(p->queue, p);
#endif
      p->state = RUNNABLE;
    }
}
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

int kill(int pid)
{
  struct proc *p;
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
#ifdef MLFQ
        add_to_queue(p->queue, p);
#endif
        p->state = RUNNABLE;
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
