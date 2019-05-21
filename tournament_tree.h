//
// Created by Omer Nizri on 2019-05-04.
//

struct trnmnt_tree {
    int *locks;
    int depth;
} typedef trnmnt_tree;

trnmnt_tree *trnmnt_tree_alloc(int depth);

int trnmnt_tree_dealloc(trnmnt_tree *tree);

int trnmnt_tree_acquire(trnmnt_tree *tree, int id);

int trnmnt_tree_release(trnmnt_tree *tree, int id);
