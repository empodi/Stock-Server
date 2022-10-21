/* $begin sbuf.h */
#ifndef __SBUF_H__
#define __SBUF_H__

#include "semaphore.h"

typedef struct {
    int *buf;           /* Buffer array */
    int n;              /* Maximum number of slots */
    int front;          /* buf[(front + 1) % n] is first item */     
    int rear;           /* buf[(rear % n)] is last item */
    sem_t mutex;        /* Protects accesses to buf */
    sem_t slots;        /* Counts available slots */
    sem_t items;        /* Counts available items */
} sbuf_t;

#endif /* __SBUF_H__ */
/* $end sbuf.h */
