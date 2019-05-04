#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void) {
    return fork();
}

int
sys_exit(void) {
    exit();
    return 0;  // not reached
}

int
sys_wait(void) {
    return wait();
}

int
sys_kill(void) {
    int pid;

    if (argint(0, &pid) < 0)
        return -1;
    return kill(pid);
}

int
sys_getpid(void) {
    return myproc()->pid;
}

int
sys_sbrk(void) {
    int addr;
    int n;

    if (argint(0, &n) < 0)
        return -1;
    addr = myproc()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

int
sys_sleep(void) {
    int n;
    uint ticks0;

    if (argint(0, &n) < 0)
        return -1;
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n) {
        if (myproc()->killed) {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void) {
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}


/**
Threads
 */

int sys_kthread_create(void) {
    void *stack;
    void (*start_func)();
    argptr(0, (void *) &start_func, sizeof(start_func));
    argptr(1, (void *) &stack, sizeof(stack));
    return kthread_create(start_func, stack);
}

int sys_kthread_id(void) {
    return kthread_id();
}

int sys_kthread_exit(void) {
    kthread_exit();
    // should not reach the following line
    return 0;
}

int sys_kthread_join(void) {
    int pid;
    if (argint(0, &pid) < 0)
        return -1;
    return kthread_join(pid);
}

int sys_kthread_mutex_alloc(void) {
    return kthread_mutex_alloc();
}

int sys_kthread_mutex_dealloc(void) {
    int mutex_id;
    if (argint(0, &mutex_id) < 0)
        return -1;
    return kthread_mutex_dealloc(mutex_id);
}

int sys_kthread_mutex_lock(void) {
    int mutex_id;
    if (argint(0, &mutex_id) < 0)
        return -1;
    return kthread_mutex_lock(mutex_id);
}

int sys_kthread_mutex_unlock(void) {
    int mutex_id;
    if (argint(0, &mutex_id) < 0)
        return -1;
    return kthread_mutex_unlock(mutex_id);
}