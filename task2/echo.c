/*
 * echo - read and echo text lines until client closes connection
 */
/* $begin echo */
#include "csapp.h"
#include "stock.h"

extern void searchAndUpdate(int targetId, int amount, bool action, const int connfd, char* cmd);
extern void createResultString(TreeNode* node, char* newBuf);
int parseline(char* buf, char** argv);

extern sem_t mutex;
extern TreeNode* root;

void echo(int connfd) {

    int n; 
    char buf_copy[20];
    char request[20];
    char buf[MAXLINE]; 
    rio_t rio;

    Rio_readinitb(&rio, connfd);

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {
        P(&mutex);
        strcpy(buf_copy, buf);
        buf_copy[strlen(buf_copy) - 1] = '\0';
        strcpy(request, buf_copy);

	    printf("server received %d bytes\n", n);

        buf[strlen(buf) - 1] = '\0';
        char *argv[10] = {0};
        int argc = parseline(buf, argv);

        if (!strcmp(argv[0], "show")) {

            char newBuf[MAXLINE];
            createResultString(root, newBuf);
            newBuf[strlen(newBuf)] = '\0';
            Rio_writen(connfd, newBuf, MAXLINE);
        }
        else if (argc == 3) {
            
            int action_id = atoi(argv[1]);
            int action_amount = atoi(argv[2]);
            bool flag = false;
                    
            if (!strcmp(argv[0], "sell")) {
                flag = true;
            }
            searchAndUpdate(action_id, action_amount, flag, connfd, request);
        }
        V(&mutex);
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

/* $end echo */