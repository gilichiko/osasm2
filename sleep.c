#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

int main(int argc, char *argv[]){
    int sleepAmount = 0;
    char* usageString = "Usage: sleep ticks_to_sleep\n";

    // Asserting correct number of arguments
    if(argc != 2){
        printf(2, usageString);
        exit();
    }
    
    // Retrieving amount to sleep
    sleepAmount = atoi(argv[1]);
    
    printf(1,"Sleep starting sleep !!!\n");

    sleep(sleepAmount);

    printf(1,"Sleep exiting !!!\n");
    exit();
}
