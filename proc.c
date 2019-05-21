#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"


//3.1 sturct mutex and states

enum mutex_state {
    mutex_free, mutex_locked
};
struct mutex {
    struct spinlock lock;
    int mutex_id;
    int mutex_index;
    enum mutex_state state;
    int curr_pid;
    int curr_tid;
    int in_use;
};

struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

//mutex's list

struct {
    struct spinlock lock;
    int nextmid;
    struct mutex mutexs[MAX_MUTEXES];
} mutex_list;

static struct proc *initproc;

int nextpid = 1;

extern void forkret(void);

extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void) {
    initlock(&ptable.lock, "ptable"); //initalizing ptable
    initlock(&mutex_list.lock, "mutex_list");
    for (int i = 0; i < MAX_MUTEXES; ++i) {
        struct mutex *m = &mutex_list.mutexs[i];
        m->state = mutex_free;
        m->in_use = 0;
        m->mutex_index = i;
        initlock(&(m->lock), "mutex_" + i);
    }
}

// Must be called with interrupts disabled
int
cpuid() {
    return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void) {
    int apicid, i;

    if (readeflags() & FL_IF)
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
struct proc *
myproc(void) {
    struct cpu *c;
    struct proc *p;
    pushcli();
    c = mycpu();
    p = c->proc;
    popcli();
    return p;
}

struct thread *
mythread(void) {
    struct cpu *c;
    struct thread *thread;
    pushcli();
    c = mycpu();
    thread = c->currthread;
    popcli();
    return thread;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void) {
    struct proc *p;
    struct thread *t;
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

    release(&ptable.lock);
    t = (struct thread *) p->threads;
    // Allocate kernel stack.
    if ((t->kstack = kalloc()) == 0) {
        p->state = UNUSED;
        return 0;
    }
    sp = t->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *t->tf;
    t->tf = (struct trapframe *) sp;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    sp -= 4;
    *(uint *) sp = (uint) trapret;

    sp -= sizeof *t->context;
    t->context = (struct context *) sp;
    memset(t->context, 0, sizeof *t->context);
    t->context->eip = (uint) forkret;
    p->nexttid = 1;
    p->state = USED;
    return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void) {
    struct proc *p;
    extern char _binary_initcode_start[], _binary_initcode_size[];

    p = allocproc();

    initproc = p;
    struct thread t = p->threads[0];
    if ((p->pgdir = setupkvm()) == 0)
        panic("userinit: out of memory?");
    inituvm(p->pgdir, _binary_initcode_start, (int) _binary_initcode_size);
    p->sz = PGSIZE;

    memset(t.tf, 0, sizeof(*t.tf));
    t.tf->cs = (SEG_UCODE << 3) | DPL_USER;
    t.tf->ds = (SEG_UDATA << 3) | DPL_USER;
    t.tf->es = t.tf->ds;
    t.tf->ss = t.tf->ds;
    t.tf->eflags = FL_IF;
    t.tf->esp = PGSIZE;
    t.tf->eip = 0;  // beginning of initcode.S

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");

    // this assignment to p->state lets other cores
    // run this process. the acquire forces the above
    // writes to be visible, and the lock is also needed
    // because the assignment might not be atomic.
    acquire(&ptable.lock);

    t.state = threadRUNNABLE;
    p->threads[0] = t;

    release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n) {
    uint sz;
    struct proc *curproc = myproc();

    sz = curproc->sz;
    if (n > 0) {
        if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
            return -1;
    } else if (n < 0) {
        if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
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
fork(void) {
    int i, pid;
    struct proc *np;
    struct thread *currthread = mythread();
    struct proc *curproc = myproc();

    // Allocate process.
    if ((np = allocproc()) == 0) {
        return -1;
    }
    struct thread *nt = &np->threads[0];

    // Copy process state from proc.
    if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0) {
        kfree(nt->kstack);
        nt->kstack = 0;
        np->state = UNUSED;
        nt->state = threadUNUSED;
        return -1;
    }
    np->sz = curproc->sz;
    np->parent = curproc;
    *nt->tf = *currthread->tf;

    // Clear %eax so that fork returns 0 in the child.
    nt->tf->eax = 0;

    for (i = 0; i < NOFILE; i++)
        if (curproc->ofile[i])
            np->ofile[i] = filedup(curproc->ofile[i]);
    np->cwd = idup(curproc->cwd);

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    pid = np->pid;

    acquire(&ptable.lock);

    np->state = USED;
    nt->state = threadRUNNABLE;
    release(&ptable.lock);

    return pid;
}


void deallocate_process_mutexes(struct proc *p) {
    acquire(&mutex_list.lock);
    for (int i = 0; i < MAX_MUTEXES; ++i) {
        struct mutex *m = &mutex_list.mutexs[i];
        acquire(&m->lock);
        if (m->in_use == 1 && p->pid == m->curr_pid) {
            m->in_use = 0;
            m->mutex_index = -1;
            m->state = mutex_free;
            m->curr_pid = -1;
            m->curr_tid = -1;
            m->mutex_id = -1;
        }
        release(&m->lock);
    }
    release(&mutex_list.lock);
}

void unlock_thread_mutexes(struct proc *p, struct thread *t) {
    acquire(&mutex_list.lock);
    for (int i = 0; i < MAX_MUTEXES; ++i) {
        struct mutex *m = &mutex_list.mutexs[i];
        acquire(&m->lock);
        if (m->in_use == 1 && p->pid == m->curr_pid && t->tid == m->curr_tid && m->state == mutex_locked) {
            m->state = mutex_free;
        }
        release(&m->lock);
    }
    release(&mutex_list.lock);
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void) {
    acquire(&ptable.lock);

    struct proc *curproc = myproc();
    struct proc *p;
    struct thread *t;
    int fd;

    if (curproc == initproc) {
        panic("init exiting");
    }


    struct thread *curthread = mythread();


    curthread->state = threadZOMBIE;
    curproc->killed = 1;

    // check if there's another thread with runnable / running process

    int exist_running_thread = 0;
    for (t = curproc->threads; t < &curproc->threads[NTHREAD]; t++) {
        // TODO: check if need sleeping
        if (t->state != threadUNUSED && t->state != threadZOMBIE) {
            exist_running_thread = 1;
        }
    }

    if (!exist_running_thread) {
        release(&ptable.lock);

        // Close all open files.
        for (fd = 0; fd < NOFILE; fd++) {
            if (curproc->ofile[fd]) {
                fileclose(curproc->ofile[fd]);
                curproc->ofile[fd] = 0;
            }
        }


        begin_op();
        iput(curproc->cwd);
        end_op();
        curproc->cwd = 0;
        deallocate_process_mutexes(myproc());


        acquire(&ptable.lock);

        // Parent might be sleeping in wait().
        wakeup1(curproc->parent);

        // Pass abandoned children to init.
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->parent == curproc) {
                p->parent = initproc;
                if (p->state == ZOMBIE)
                    wakeup1(initproc);
            }
        }

        // Jump into the scheduler, never to return.
        curproc->state = ZOMBIE;
        curthread->state = threadZOMBIE;
    }
    wakeup1(curthread);
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void) {
    struct proc *p;
    int havekids, pid;
    struct proc *curproc = myproc();
    //fixme mybe should use- struct thread *currthread=mythread();
    struct thread *t;
    acquire(&ptable.lock);
    for (;;) {
        // Scan through table looking for exited children.
        havekids = 0;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->parent != curproc) {
                continue;
            }
            havekids = 1;
            if (p->state == ZOMBIE) {
                // Found one.
                // we should free all the threads
                for (t = p->threads; t < &p->threads[NTHREAD]; t++) {
                    if (t->state != threadUNUSED) {
                        kfree(t->kstack);
                        t->tid = -1;
                        t->context = 0;
                        t->tf = 0;
                        t->chan = 0;
                        t->kstack = 0;
                        t->state = threadUNUSED;
                        t->killed = 0;
                    }
                }

                pid = p->pid;
                freevm(p->pgdir);
                p->pid = 0;
                p->parent = 0;
                p->name[0] = 0;
                p->killed = 0;
                p->state = UNUSED;

                release(&ptable.lock);
                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || curproc->killed) {
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(curproc, &ptable.lock);  //DOC: wait-sleep
        //todo sleep for thread? for all threads
    }
}

int is_runnable_thread_exist(struct proc *p) {
    struct thread *t;
    int position = 0;
    for (t = p->threads; t < &p->threads[NTHREAD]; t++) {
        if (t->state == threadRUNNABLE) {
            return position;
        }
        position++;
    }
    return -1;
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
scheduler(void) {
    struct proc *p;
    struct cpu *c = mycpu();
    c->proc = 0;

    for (;;) {
        // Enable interrupts on this processor.
        sti();

        // Loop over process table looking for process to run.
        acquire(&ptable.lock);
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != USED)
                continue;
            int thread_position = is_runnable_thread_exist(p);
            if (thread_position == -1)
                continue;
            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            c->proc = p;


            struct thread *t = &p->threads[thread_position];
            c->currthread = t;

            switchuvm(p);
            t->state = threadRUNNING;

            swtch(&(c->scheduler), t->context);
            switchkvm();


            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
            c->currthread = 0;
        }
        release(&ptable.lock);

    }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void) {
    int intena;
    struct thread *t = mythread();

    if (!holding(&ptable.lock))
        panic("sched ptable.lock");
    if (mycpu()->ncli != 1)
        panic("sched locks");
    if (t->state == threadRUNNING)
        panic("sched running");
    if (readeflags() & FL_IF)
        panic("sched interruptible");
    intena = mycpu()->intena;
    swtch(&t->context, mycpu()->scheduler);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void) {
    acquire(&ptable.lock);  //DOC: yieldlock
    mythread()->state = threadRUNNABLE;
    sched();
    release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void) {
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

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk) {
    struct thread *t = mythread();

    if (t == 0)
        panic("sleep");

    if (lk == 0)
        panic("sleep without lk");

    // Must acquire ptable.lock in order to
    // change p->state and then call sched.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.
    if (lk != &ptable.lock) {  //DOC: sleeplock0
        acquire(&ptable.lock);  //DOC: sleeplock1
        release(lk);
    }
    // Go to sleep.
    t->chan = chan;
    t->state = threadSLEEPING;

    sched();

    // Tidy up.
    t->chan = 0;

    // Reacquire original lock.
    if (lk != &ptable.lock) {  //DOC: sleeplock2
        release(&ptable.lock);
        acquire(lk);
    }
}

int next_tid_to_join() {
    acquire(&ptable.lock);
    struct thread *curthread = mythread();
    struct proc *curproc = myproc();
    struct thread *t;
    for (t = curproc->threads; t < &curproc->threads[NTHREAD]; t++) {
        if (t->tid != curthread->tid && t->state != threadUNUSED) {
            release(&ptable.lock);
            return t->tid;
        }
    }
    release(&ptable.lock);
    return -1;
}

void kill_other_threads() {
    acquire(&ptable.lock);
    struct thread *curthread = mythread();
    struct proc *curproc = myproc();
    struct thread *t;
    for (t = curproc->threads; t < &curproc->threads[NTHREAD]; t++) {
        if (t->tid != curthread->tid) {
            t->killed = 1;
        }
        if (t->state == threadSLEEPING) {
            t->state = threadRUNNABLE;
        }
    }
    release(&ptable.lock);
    int next_tid;
    while ((next_tid = next_tid_to_join()) != -1) {
        kthread_join(next_tid);
    }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan) {
    struct proc *p;
    struct thread *t;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        for (t = p->threads; t < &p->threads[NTHREAD]; t++) {
            if (t->state == threadSLEEPING && t->chan == chan)
                t->state = threadRUNNABLE;
        }
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan) {
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid) {
    struct proc *p;
    struct thread *t;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            p->killed = 1;
            // Wake process from sleep if necessary.
            for (t = p->threads; t < &p->threads[NTHREAD]; t++) {
                if (t->state == threadSLEEPING)
                    t->state = threadRUNNABLE;
            }
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
procdump(void) {
    static char *states[] = {
            [USED]    "used",
            [UNUSED]    "unused",
            [EMBRYO]    "embryo",
//            [SLEEPING]  "sleep ",
//            [RUNNABLE]  "runble",
//            [RUNNING]   "run   ",
            [ZOMBIE]    "zombie"
    };
    int i;
    struct proc *p;
    struct thread *t;
    char *state;
    uint pc[10];

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        cprintf("%d %s %s", p->pid, state, p->name);
        for (t = p->threads; t < &p->threads[NTHREAD]; t++) {
            if (t->state == threadSLEEPING) {
                getcallerpcs((uint *) t->context->ebp + 2, pc);
                for (i = 0; i < 10 && pc[i] != 0; i++)
                    cprintf(" %p", pc[i]);
            }
        }
        cprintf("\n");
    }
}


/**
 * Threads
 */


int get_next_free_thread(struct proc *p) {
    struct thread *t;
    int position = 0;
    for (t = p->threads; t < &p->threads[NTHREAD]; t++) {
        if (t->state == threadUNUSED) {
            return position;
        }
        position++;
    }
    return -1;
}

int kthread_create(void (*start_func)(), void *stack) {
    acquire(&ptable.lock);
    struct proc *p = myproc();
    int free_thread_position = get_next_free_thread(p);
    if (free_thread_position == -1) {
        release(&ptable.lock);
        return -1;
    }

    struct thread *t = &p->threads[free_thread_position];

    t->tid = p->nexttid++;

    if ((t->kstack = kalloc()) == 0) {
        release(&ptable.lock);
        return -1;
    }


    char *sp;
    sp = t->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *t->tf;
    t->tf = (struct trapframe *) sp;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    sp -= 4;
    *(uint *) sp = (uint) trapret;

    sp -= sizeof *t->context;
    t->context = (struct context *) sp;
    memset(t->context, 0, sizeof *t->context);
    t->context->eip = (uint) forkret;


    *t->tf = *mythread()->tf;


    t->tf->eip = (uint) start_func;
    t->tf->esp = ((uint) stack);

    t->state = threadRUNNABLE;
    release(&ptable.lock);
    return t->tid;
}

int kthread_id(void) {
    return mythread()->tid;
}

void kthread_exit(void) {
    acquire(&ptable.lock);
    struct proc *curproc = myproc();
    struct thread *curthread = mythread();
    struct thread *t;

    unlock_thread_mutexes(curproc, curthread);

    int exist_running_thread = 0;
    for (t = curproc->threads; t < &curproc->threads[NTHREAD]; t++) {
        if (t->state != threadUNUSED && t->state != threadZOMBIE && t->tid != curthread->tid) {
            exist_running_thread = 1;
        }
    }

    if (exist_running_thread) {
        curthread->state = threadZOMBIE;
        wakeup1(curthread);
        sched();
    } else {
        release(&ptable.lock);
        exit();
    }
}

int kthread_join(int thread_id) {
    if (thread_id < 0)
        return -1;
    acquire(&ptable.lock);
    struct proc *curproc = myproc();
    struct thread *t;
    struct thread *request_thread;
    int tid_exists = 0;


    for (t = curproc->threads; t < &curproc->threads[NTHREAD]; t++) {
        if (t->tid == thread_id) {
            tid_exists = 1;
            request_thread = t;
            if (t->state == threadZOMBIE) {
                t->state = threadUNUSED;
                t->killed = 0;
                t->tid = -1;
                kfree(t->kstack);
                t->kstack = 0;
                request_thread->chan = 0;
                request_thread->tf = 0;
                request_thread->context = 0;
                release(&ptable.lock);
                return 0;
            }
        }
    }


    if (!tid_exists) {
        release(&ptable.lock);
        return -1;
    }
    // if arrived to this line, it indicates that thread exist but still running and we shall wait.
    sleep(request_thread, &ptable.lock);

    request_thread->state = threadUNUSED;
    request_thread->killed = 0;
    kfree(request_thread->kstack);
    request_thread->tid = -1;
    request_thread->kstack = 0;
    request_thread->chan = 0;
    request_thread->tf = 0;
    request_thread->context = 0;
    release(&ptable.lock);
    return 0;
}

int next_mid(int for_index) {
    int ret;
    acquire(&mutex_list.lock);
    ret = mutex_list.nextmid++;
    release(&mutex_list.lock);
    return ret;
}

int is_availble_for_dealloc_mutex(int needed_mutex_id) {
    acquire(&mutex_list.lock);
    for (int i = 0; i < MAX_MUTEXES; ++i) {
        acquire(&mutex_list.mutexs[i].lock);
        if (mutex_list.mutexs[i].mutex_id == needed_mutex_id) {
            if (mutex_list.mutexs[i].in_use != 1 || mutex_list.mutexs[i].state == mutex_locked) {
                release(&mutex_list.lock);
                release(&mutex_list.mutexs[i].lock);
                return -1;
            } else {
                /*if (mutex_list.mutexs[i].curr_pid != myproc()->pid ||
                    mutex_list.mutexs[i].curr_tid != mythread()->tid) {
                    release(&mutex_list.lock);
                    release(&mutex_list.mutexs[i].lock);
                    return -1;
                }*/
                release(&mutex_list.lock);
                return i;
            }
        }
        release(&mutex_list.mutexs[i].lock);
    }
    release(&mutex_list.lock);
    return -1;
}

int next_availble_mutex() {
    acquire(&mutex_list.lock);
    for (int i = 0; i < MAX_MUTEXES; ++i) {
        acquire(&mutex_list.mutexs[i].lock);
        if (mutex_list.mutexs[i].in_use == 0) {
            release(&mutex_list.lock);
            return i;
        }
        release(&mutex_list.mutexs[i].lock);
    }
    release(&mutex_list.lock);
    return -1;
}

//3.1 mutex functions
int kthread_mutex_alloc() {
    int this_mutex_index = next_availble_mutex();
    if (this_mutex_index == -1) return -1;
    else {
        struct mutex *m = &mutex_list.mutexs[this_mutex_index];
        m->in_use = 1;
        m->mutex_index = this_mutex_index;
        m->state = mutex_free;
        m->curr_pid = myproc()->pid;
        m->curr_tid = mythread()->tid;
        m->mutex_id = next_mid(this_mutex_index);
        release(&m->lock);
        return m->mutex_id;
    }
}

int kthread_mutex_dealloc(int mutex_id) {
    int this_mutex_index = is_availble_for_dealloc_mutex(mutex_id);
    if (this_mutex_index == -1) return -1;
    else {
        struct mutex *m = &mutex_list.mutexs[this_mutex_index];
        m->in_use = 0;
        m->mutex_index = -1;
        m->state = mutex_free;
        m->curr_pid = -1;
        m->curr_tid = -1;
        m->mutex_id = -1;
        release(&m->lock);
        return 0;
    }
}

void
acquiremutex(struct mutex *lk) {
    while (lk->state == mutex_locked) {
        sleep(lk, &lk->lock);
    }
    acquire(&ptable.lock);
    lk->state = mutex_locked;
    lk->curr_pid = myproc()->pid;
    lk->curr_tid = mythread()->tid;
    release(&ptable.lock);
}

void
releasemutex(struct mutex *lk) {
    lk->state = mutex_free;
    lk->curr_pid = 0;
    lk->curr_tid = 0;
    wakeup(lk);
}


int kthread_mutex_lock(int needed_mutex_id) {
    acquire(&mutex_list.lock);
    for (int i = 0; i < MAX_MUTEXES; ++i) {
        acquire(&mutex_list.mutexs[i].lock);
        if (mutex_list.mutexs[i].mutex_id == needed_mutex_id) {
            release(&mutex_list.lock);
            acquiremutex(&mutex_list.mutexs[i]);
            // we're holding the mutex
            release(&mutex_list.mutexs[i].lock);
            return needed_mutex_id;
        }
        release(&mutex_list.mutexs[i].lock);
    }
    release(&mutex_list.lock);
    return -1;
}

int kthread_mutex_unlock(int needed_mutex_id) {
    acquire(&mutex_list.lock);
    for (int i = 0; i < MAX_MUTEXES; ++i) {
        struct mutex *m = &mutex_list.mutexs[i];
        acquire(&m->lock);
        if (m->mutex_id == needed_mutex_id) {
            if (mythread()->tid == m->curr_tid && myproc()->pid == m->curr_pid) {
                release(&mutex_list.lock);
                releasemutex(m);
                // we're holding the mutex
                release(&m->lock);
                return needed_mutex_id;
            }
        }
        release(&m->lock);
    }
    release(&mutex_list.lock);
    return -1;
}
