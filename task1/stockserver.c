/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "stdbool.h"
#include "csapp.h"
#include "stock.h"

typedef struct { /* Represents a pool of connected descriptors */

    int maxfd;          /* Largest descriptor in read_set */
    fd_set read_set;    /* Set of all active descriptors */
    fd_set ready_set;   /* Subset of descriptors ready for reading */
    int nready;         /* Number of ready descriptors from select */
    int maxi;           /* High water index into client array */
    int clientfd[FD_SETSIZE];       /* Set of active descriptors */
    rio_t clientrio[FD_SETSIZE];    /* Set of active read buffers */
} pool;

TreeNode* root = NULL;
int byte_cnt = 0;

void echo(int connfd);
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients (pool *p);
TreeNode* createNode(int id, int amount, int price);
void addNodeToTree(TreeNode* node);
void deleteTree(TreeNode* node);
void searchAndUpdate(int targetId, int amount, bool action, const int connfd, char* cmd);
void createResultString(TreeNode* node, char* newBuf);
int parseline(char* buf, char** argv);

int main(int argc, char **argv) {

    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    static pool pool;
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
    init_pool(listenfd, &pool);

    while (1) {
	    /* Wait for listening or connected descriptor(s) to become ready */
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

        /* If listening descriptor ready, add new client to pool */
        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
            add_client(connfd, &pool);
        }

        /* Echo a text line from each ready connected descriptor */
        check_clients(&pool);
    }

    deleteTree(root);
    exit(0);
}
/* $end echoserverimain */

TreeNode* createNode(int id, int amount, int price) {

    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    node->stockItem.id = id;
    node->stockItem.amount = amount;
    node->stockItem.price = price;
    node->left = node->right = NULL;

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
            } else {
                temp = temp->left;
            }
        } else {
            if (temp->right == NULL) {
                temp->right = node;
                break;
            } else {
                temp = temp->right;
            }
        }
    }
    return;
}

void init_pool(int listenfd, pool *p) {

    /* Initially, there are no connected descriptors */
    int i;
    p->maxi = -1;
    
    for (i = 0; i < FD_SETSIZE; i++) {
        p->clientfd[i] = -1;
    }

    /* Initially, listenfd is only member of select read set */
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p) {

    int i;
    p->nready--;

    for (i = 0; i < FD_SETSIZE; i++) {

        if (p->clientfd[i] < 0) {

            /* Add connected descriptor to the pool */
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            /* Add the descriptor to descriptor set */
            FD_SET(connfd, &p->read_set);

            /* Update max descriptor and pool high water mark */
            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }

    if (i == FD_SETSIZE) {  /* Couldn't find an empty slot */
        app_error("add_client error: Too many clients");
    }
}

int parseline(char* buf, char** argv) {

    int argc = 0;
    char delim[] = " ";
    char* result;

    result = strtok(buf, delim);

    while (result) {
        argv[argc++] = result;
        result = strtok(NULL, delim);
    }

    return argc;
}

void searchAndUpdate(int targetId, int amount, bool action, const int connfd, char* cmd) {   // action: sell if true, buy if false

    char buf[MAXLINE];
    TreeNode* node = root;
    bool updated = false;

    while (node) {
        if (targetId == node->stockItem.id) {
            if (action || node->stockItem.amount >= amount) {   // sell or buy stock
                if (action) {
                    node->stockItem.amount += amount;
                    sprintf(buf, "[sell] success\n");
                } else {
                    node->stockItem.amount -= amount;
                    sprintf(buf, "[buy] success\n");
                }
                updated = true;
            }
            break;
        } else if (targetId < node->stockItem.id) {
            node = node->left;
        } else {
            node = node->right;
        }
    }

    if (!updated) {
        sprintf(buf, "Not enough left stock\n");
    }
    Rio_writen(connfd, buf, MAXLINE);
    return;
}

void createResultString(TreeNode* node, char* newBuf) {

    if (!node) return;

    char tmp[10];

    createResultString(node->left, newBuf);

    sprintf(tmp, "%d", node->stockItem.id);
    strcat(newBuf, tmp);
    strcat(newBuf, " ");
    sprintf(tmp, "%d", node->stockItem.amount);
    strcat(newBuf, tmp);
    strcat(newBuf, " ");
    sprintf(tmp, "%d", node->stockItem.price);
    strcat(newBuf, tmp);
    strcat(newBuf, "\n");

    createResultString(node->right, newBuf);
}

void check_clients (pool *p) {

    int i, connfd, n;
    char cmd_experiment[20];
    char buf_copy[20];
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {

        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        /* If the descriptor is ready, echo a text line from it */
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {

            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
                byte_cnt += n;

                strcpy(buf_copy, buf);
                buf_copy[strlen(buf_copy) - 1] = '\0';
                strcpy(cmd_experiment, buf_copy);
                printf("Server received %d (%d total) bytes on fd: %d\n", n, byte_cnt, connfd);

                char *argv[10] = {0};
                int argc = parseline(buf_copy, argv);
                
                if (!strcmp(argv[0], "show")) {

                    char newBuf[MAXLINE];
                    memset(newBuf, '\0', sizeof(newBuf));
                    createResultString(root, newBuf);
                    Rio_writen(connfd, newBuf, MAXLINE);
                }
                else if (argc == 3) {
                    int action_id = atoi(argv[1]);
                    int action_amount = atoi(argv[2]);
                    bool flag = false;

                    if (!strcmp(argv[0], "sell")) {
                        flag = true;
                    }
                    searchAndUpdate(action_id, action_amount, flag, connfd, cmd_experiment);
                }
            }
            else {  /* EOF detected, remove descriptor from pool */
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;

                /* File (stock.txt) write start */
                FILE *fp = fopen("stock.txt", "w");
                if (!fp) {
                    fprintf(stderr, "The file (stock.txt) does not exist. \n");
                    return;
                }
                
                char res[MAXLINE] = {0};
                createResultString(root, res);
                fprintf(fp, "%s", res);
                fclose(fp);
                /* File write end */
            }
        }
    }
}