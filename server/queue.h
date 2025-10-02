#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <bits/pthreadtypes.h>

typedef struct node_{
    int fd;
    struct node_* next;
} node_t;

typedef struct {
    int size;
    node_t* head;
    node_t* tail;

    pthread_mutex_t mutex_queue;
    pthread_cond_t cond_new_fd;
} queue_t;

// 初始化队列。
void init_queue(queue_t* queue);

// 销毁队列。
void destroy_queue(queue_t* queue);

// 将 fd 入队到队列中。
void enqueue(queue_t* queue, int fd);

// 从队列中出队，返回 fd。
int dequeue(queue_t* queue);

/*
 * 功能：子线程要做的工作。
 * 参数：客户端的连接。
 * 返回值：成功返回 0，失败返回非零值。
 */
int do_work(int client_fd);

#endif //QUEUE_H
