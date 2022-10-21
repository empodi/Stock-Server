/* $begin stock.h */
#ifndef __STOCK_H__
#define __STOCK_H__

/* Definition for a stock item */
typedef struct _stock_ {
    int id;
    int price;
    int amount;
} stock;

/* Definition for a binary tree node */
typedef struct _treeNode_ {
    stock stockItem;
    struct _treeNode_ *left;
    struct _treeNode_ *right;
} TreeNode;

#endif /* __STOCK_H__ */
/* $end stock.h */