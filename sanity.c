#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "tournament_tree.h"
#include "kthread.h"

#define STACK_SIZE 100

void trnmt_test(){
    printf(1,"4 ttree starting\n");
    trnmnt_tree* tree;
    int result;
    tree = trnmnt_tree_alloc(4);
    if(tree == 0){
        printf(1,"4 ttree allocated unsuccessfully\n");
    }
    result = trnmnt_tree_acquire(tree, 1);
    if(result < 0){
        printf(1,"ttree locked unsuccessfully\n");
    }
    result = trnmnt_tree_release(tree, 1);
    if(result < 0){
        printf(1,"ttree unlocked unsuccessfully\n");
    }
    result = trnmnt_tree_dealloc(tree);
    if(result == -1){
        printf(1,"1 ttree deallocated unsuccessfully\n");
    }
    printf(1,"ttree test and all good \n");
    //kthread_exit();
}
void func_for_thread1(void) {
    for (int i = 0; i <1000 ; ++i) {
        sleep(10);
    }
    kthread_exit();
}
void func_for_thread2(void) {
    for (int i = 0; i <1000 ; ++i) {
        sleep(1);
    }
    kthread_exit();
}

void thread_t1(){
    void *stack_t1 = ((char *) malloc(STACK_SIZE * sizeof(char))) + STACK_SIZE;
    void (*start_t1)(void) = func_for_thread1;
    int t1_id = kthread_create(start_t1, stack_t1);
    if(t1_id == -1) {
        printf(1,"kthread_create func has failed\n");
        return;
    }
    else {
        printf(1,"kthread_create func has succeeded \n");
    }
}
void thread_t2(){
    void *stack_t1 = ((char *) malloc(STACK_SIZE * sizeof(char))) + STACK_SIZE;
    void *stack_t2 = ((char *) malloc(STACK_SIZE * sizeof(char))) + STACK_SIZE;
    void (*start_t1)(void) = func_for_thread1;
    void (*start_t2)(void) = func_for_thread2;
    int t1_id = kthread_create(start_t1, stack_t1);
    kthread_join(t1_id);
    printf(1,"thread 1 finished, join is working \n");
    int t2_id = kthread_create(start_t2, stack_t2);
    if(t1_id == -1||t2_id==-1) {
        printf(1,"kthread_create func has failed\n");
        return;
    }
    else {
        printf(1,"kthread_create func has succeeded \n");
    }
}
int main(){
    printf(1,"sanity tests starting \n");
    trnmt_test();
    thread_t1();
    thread_t2();
    printf(1,"sanity tests ending \n");
    exit();
}