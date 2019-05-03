//
// Created by Omer Nizri on 2019-04-30.
//

#include "types.h"
#include "x86.h"
#include "user.h"
#include "kthread.h"

#define THREAD_NUM 1
#define STACK_SIZE 500

#define THREAD_START(name, id) \
    void name(){ \
        printf(1, "thread %d entering\n", id); \
        sleep( id * 100); \
        printf(1, "thread %d exiting\n", id); \
        kthread_exit(); \
    }

#define THREAD_STACK(name) \
    void * name = ((char *) malloc(STACK_SIZE * sizeof(char))) + STACK_SIZE;

THREAD_START(threadStart_1, 1)

void (*threads_starts[])(void) =
        {threadStart_1};

int main(int argc, char *argv[]){
    THREAD_STACK(threadStack_1)

    void (*threads_stacks[])(void) =
            {threadStack_1};

    for(int i = 0;i < THREAD_NUM;i++){
        kthread_create(threads_starts[i], threads_stacks[i]);
    }

    sleep(1000);

    exit();
}