#define MAX_STACK_SIZE 4000
#define MAX_MUTEXES 64

/********************************
        The API of the KLT package
 ********************************/

int kthread_create(void (*start_func)(), void* stack);
int kthread_id();
void kthread_exit();
int kthread_join(int thread_id);

int kthread_mutex_alloc();
int kthread_mutex_dealloc(int mutex_id);
int kthread_mutex_lock(int mutex_id);
int kthread_mutex_unlock(int mutex_id);

/* trnmnt_tree* trnmnt_tree_alloc(int depth);
int trnmnt_tree_dealloc(trnmnt_tree* tree);
int trnmnt_tree_acquire(trnmnt_tree* tree,int ID);
int trnmnt_tree_release(trnmnt_tree* tree,int ID); */

enum threadstate { threadUNUSED, threadEMBRYO, threadSLEEPING, threadRUNNABLE, threadRUNNING, threadZOMBIE };

struct thread {
    enum threadstate state; //the thread's state
    int tid;                     //the threads id
    char *kstack;                // Bottom of kernel stack for this process
    void *chan;                  // If non-zero, sleeping on chan
    struct trapframe *tf;        // Trap frame for current syscall
    struct context *context;     // swtch() here to run process
    struct proc *proc;           // The father procces of this thread
};