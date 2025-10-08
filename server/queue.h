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

#endif //QUEUE_H
