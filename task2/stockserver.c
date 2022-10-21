/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#include "sbuf.h"
#include "stock.h"

sem_t mutex;
sbuf_t sbuf;            /* Shared buffer of connected descriptors */
TreeNode* root = NULL;
int writeCnt = 0;

void echo(int connfd);
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
void *thread(void *vargp);
void deleteTree(TreeNode* node);
TreeNode* createNode(int id, int amount, int price);
void addNodeToTree(TreeNode* node);

void searchAndUpdate(int targetId, int amount, bool action, const int connfd, char* cmd);
void createResultString(TreeNode* node, char* newBuf);

int main(int argc, char **argv) {

    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    pthread_t tid;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    FILE *fp;

    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }

    /* File (stock.txt) read start */
    fp = fopen("stock.txt", "r");
    if (!fp) {
        fprintf(stderr, "The file (stock.txt) does not exist. \n");
        return 0;
    }
    int s_id, s_amount, s_price;
    while (fscanf(fp, "%d %d %d", &s_id, &s_amount, &s_price) != -1) {
        //printf("%d %d %d \n", s_id, s_amount, s_price);
        TreeNode* node = createNode(s_id, s_amount, s_price);
        if (!node) {
            fclose(fp);
            exit(-1);
        }
        addNodeToTree(node);
    }
    
    fclose(fp);
    /* File read end */

    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    Sem_init(&mutex, 0, 1);

    for (i = 0; i < NTHREADS; i++) {    /* Create worker threads */
        Pthread_create(&tid, NULL, thread, NULL);
    }

    while (1) {
	    clientlen = sizeof(struct sockaddr_storage); 
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        sbuf_insert(&sbuf, connfd);     /* Insert connfd in buffer */
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
    }

    sbuf_deinit(&sbuf);
    deleteTree(root);
    exit(0);
}
/* $end echoserverimain */

void searchAndUpdate(int targetId, int amount, bool action, const int connfd, char* cmd) {   // action: sell if true, buy if false

    char buf[MAXLINE];
    TreeNode* node = root;
    bool updated = false;

    while (node) {
        
        if (targetId == node->stockItem.id) {
            if (action || node->stockItem.amount >= amount) {   // sell or buy stock

                P(&node->stockItem.w);
                if (action) {
                    node->stockItem.amount += amount;
                    sprintf(buf, "[sell] success\n");
                }
                else {
                    node->stockItem.amount -= amount;
                    sprintf(buf, "[buy] success\n");
                }
                V(&node->stockItem.w);
                updated = true;
                //fprintf(stdout, "%s success \n", cmd);
                //fflush(stdout);
            }
            break;
        }
        else if (targetId < node->stockItem.id) {
            node = node->left;
        }
        else {
            node = node->right;
        }
    }

    //if (!node) {
        //sprintf(buf, "Invalid stock ID\n");
    //}
    if (!updated) {
        sprintf(buf, "Not enough left stock\n");
    }
    Rio_writen(connfd, buf, MAXLINE);

    return;
}

void createResultString(TreeNode* node, char* newBuf) {

    if (!node) return;

    createResultString(node->left, newBuf);

    P(&(node->stockItem.mutex));
    (node->stockItem.readcnt)++;
    if (node->stockItem.readcnt == 1) /* First in */
        P(&(node->stockItem.w));
    V(&(node->stockItem.mutex));

    char tmp[10];
    sprintf(tmp, "%d", node->stockItem.id);
    strcat(newBuf, tmp);
    strcat(newBuf, " ");
    sprintf(tmp, "%d", node->stockItem.amount);
    strcat(newBuf, tmp);
    strcat(newBuf, " ");
    sprintf(tmp, "%d", node->stockItem.price);
    strcat(newBuf, tmp);
    strcat(newBuf, "\n");

    P(&(node->stockItem.mutex));
    (node->stockItem.readcnt)--;
    if (node->stockItem.readcnt == 0) /* Last out */
        V(&(node->stockItem.w));
    V(&(node->stockItem.mutex));

    createResultString(node->right, newBuf);
}

TreeNode* createNode(int id, int amount, int price) {

    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    node->stockItem.id = id;
    node->stockItem.amount = amount;
    node->stockItem.price = price;
    node->left = node->right = NULL;
    
    node->stockItem.readcnt = 0;
    Sem_init(&node->stockItem.mutex, 0, 1);
    Sem_init(&node->stockItem.w, 0, 1);

    return node;
}

void deleteTree(TreeNode* node) {

    if (!node) return;

    deleteTree(node->left);
    deleteTree(node->right);
    //printf("Delete node %d \n", node->stockItem.id);
    free(node);
}

void addNodeToTree(TreeNode* node) {

    if (root == NULL) {
        root = node;
        return;
    }

    TreeNode* temp = root;
    int newId = node->stockItem.id;

    while (true) {
        
        if (newId == temp->stockItem.id) {
            printf("stock id: %d - already exists. \n", newId);
            return;
        }

        if (newId < temp->stockItem.id) {
            if (temp->left == NULL) {
                temp->left = node;
                break;
            }
            else {
                temp = temp->left;
            }
        }
        else {
            if (temp->right == NULL) {
                temp->right = node;
                break;
            }
            else {
                temp = temp->right;
            }
        }
    }

    return;
}

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n) {

    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp) {
    Free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item) {

    P(&sp->slots);                          /* Wait for available slot */
    P(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear) % (sp->n)] = item; /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->items);                          /* Announce available item */
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp) {

    int item;                               
    P(&sp->items);                              /* Wait for available item */
    P(&sp->mutex);                              /* Lock the buffer */
    item = sp->buf[(++sp->front) % (sp->n)];    /* Remove the item */    
    V(&sp->mutex);                              /* Unlock the buffer */
    V(&sp->slots);                              /* Announce available slot */

    return item;
}

void *thread(void *vargp) {

    Pthread_detach(pthread_self());

    while (1) {

        int connfd = sbuf_remove(&sbuf);    /* Remove connfd from buffer */
        //int val;

        //P(&mutex);
        //sem_getvalue(&mutex, &val);
        //printf("\nAction mutex val: %d \n", val);
        echo(connfd);                       /* Service client */
        //V(&mutex);

        Close(connfd);
        
        /* File (stock.txt) write start */
        P(&mutex);
        writeCnt++;
        
        FILE* fp = fopen("stock.txt", "w");
        if (!fp) {
            fprintf(stderr, "The file (stock.txt) does not exist. \n");
        }
        else {
            char res[MAXLINE] = {0};
            createResultString(root, res);
            fprintf(fp, "%s", res);
            
            fclose(fp);
        }
        V(&mutex);
        /* File write end */
    }
}
