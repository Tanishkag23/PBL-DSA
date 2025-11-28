#include "bst.h"

BSTNode* createBSTNode(Transaction data) {
    BSTNode* newNode = (BSTNode*)malloc(sizeof(BSTNode));
    newNode->data = data;
    newNode->left = newNode->right = NULL;
    return newNode;
}

BSTNode* insertBST(BSTNode* root, Transaction data) {
    if (root == NULL) {
        return createBSTNode(data);
    }

    if (data.amount < root->data.amount) {
        root->left = insertBST(root->left, data);
    } else {
        root->right = insertBST(root->right, data);
    }

    return root;
}

void searchBST(BSTNode* root, double amount) {
    if (root == NULL) {
        return;
    }

    if (root->data.amount == amount) {
        printf("%-5d %02d/%02d/%04d   %-10.2f %-10s %-15s %-20s\n", 
               root->data.id, 
               root->data.date.day, root->data.date.month, root->data.date.year, 
               root->data.amount, 
               root->data.type, 
               root->data.category, 
               root->data.description);
    }

    if (amount < root->data.amount) {
        searchBST(root->left, amount);
    } else {
        searchBST(root->right, amount); 
    }
}

void inorderTraversal(BSTNode* root) {
    if (root != NULL) {
        inorderTraversal(root->left);
        printf("%-5d %02d/%02d/%04d   %-10.2f %-10s %-15s %-20s\n", 
               root->data.id, 
               root->data.date.day, root->data.date.month, root->data.date.year, 
               root->data.amount, 
               root->data.type, 
               root->data.category, 
               root->data.description);
        inorderTraversal(root->right);
    }
}

void freeBST(BSTNode* root) {
    if (root != NULL) {
        freeBST(root->left);
        freeBST(root->right);
        free(root);
    }
}
