//
// Created by Omer Nizri on 2019-04-30.
//

#include "types.h"
#include "x86.h"
#include "user.h"
#include "kthread.h"

void f(void) {
    printf(0, "thread: %d was here\n", kthread_id());
    kthread_exit();
}


int
main(int argc, char *argv[]) {

    if (argc < 1) {
        exit();
    }

    printf(1, "My thread id: %d\n", kthread_id());
    char stack_first[MAX_STACK_SIZE];
    char stack_second[MAX_STACK_SIZE];
    char stack_third[MAX_STACK_SIZE];
    char stack_fourth[MAX_STACK_SIZE];
    char stack_fifth[MAX_STACK_SIZE];
    kthread_create(&f, stack_first);
    kthread_create(&f, stack_second);
    kthread_create(&f, stack_third);
    kthread_create(&f, stack_fourth);
    kthread_create(&f, stack_fifth);


    printf(1, "Join returned: %d\n", kthread_join(5));

    kthread_exit();

    printf(0, "shouldn't arrive to this place\n");

    exit();
}
