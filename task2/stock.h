/* $begin stock.h */
#ifndef __STOCK_H__
#define __STOCK_H__

#include "semaphore.h"

/* Definition for a stock item */
typedef struct _stock_ {
    int id;
    int price;
    int amount;
    int readcnt;
    sem_t mutex;
    sem_t w;
} stock;

/* Definition for a binary tree node */
typedef struct _treeNode_ {
    stock stockItem;
    struct _treeNode_ *left;
    struct _treeNode_ *right;
} TreeNode;

#endif /* __STOCK_H__ */
/* $end stock.h */
