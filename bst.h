#ifndef BST_H
#define BST_H

#include "common.h"

typedef struct BSTNode {
    Transaction data;
    struct BSTNode *left, *right;
} BSTNode;

BSTNode* insertBST(BSTNode* root, Transaction data);
void searchBST(BSTNode* root, double amount);
void inorderTraversal(BSTNode* root);
void freeBST(BSTNode* root);

#endif
