//
// Created by Omer Nizri on 2019-05-04.
//

#include "types.h"
#include "tournament_tree.h"
#include "user.h"

int get_leaf_position(trnmnt_tree *t, int tid) {
    return ((1 << t->depth) - 1) + tid;
}

int get_parent_of(int id) {
    return (id - 1) / 2;
}

int get_size_of_tree(int depth) {
    return (1 << depth) - 1;
}

trnmnt_tree *trnmnt_tree_alloc(int depth) {
    trnmnt_tree *allocated_tree = malloc(sizeof(trnmnt_tree));
    int array_size = get_size_of_tree(depth);
    int *mutexes = ((int *) malloc(array_size * sizeof(int)));
    for (int i = 0; i < array_size; ++i) {
        int new_mutex =kthread_mutex_alloc();
        if(new_mutex == -1) {
            return 0;
        }
        mutexes[i] = new_mutex;
    }

    allocated_tree->depth = depth;
    allocated_tree->locks = mutexes;

    return allocated_tree;
}

int trnmnt_tree_dealloc(trnmnt_tree *t) {
    int size = get_size_of_tree(t->depth);
    for (int i = 0; i < size; ++i) {
        if (kthread_mutex_dealloc(t->locks[i]) == -1) {
            return -1;
        }
    }
    free(t->locks);
    free(t);
    return 0;
}

int trnmnt_tree_acquire(trnmnt_tree *tree, int tid) {
    int current = get_leaf_position(tree, tid);
    while (current) {
        current = get_parent_of(current);
        if (kthread_mutex_lock(tree->locks[current]) == -1) {
            return -1;
        }
    }
    return 0;
}

int get_index_of_lock(trnmnt_tree *tree, int tid, int current_depth) {
    int counter = tree->depth - current_depth;
    int current = get_leaf_position(tree, tid);
    while (counter) {
        current = get_parent_of(current);
        counter--;
    }
    return current;
}

int trnmnt_tree_release(trnmnt_tree *tree, int tid) {
    int current_depth = 0;
    while (current_depth < tree->depth) {
        int index_of_lock = get_index_of_lock(tree, tid, current_depth);
        if (kthread_mutex_unlock(tree->locks[index_of_lock]) == -1) {
            return -1;
        }
        current_depth++;
    }
    return 0;
}
