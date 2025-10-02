#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include "head.h"
#include "queue.h"

typedef struct {
    int num;
    pthread_t* threads;
} threadpool_t;

void init_threadpool(threadpool_t* threadpool, int num, queue_t* queue);
void destroy_threadpool(threadpool_t* threadpool);
void * thread_main(void* arg);

#endif //THREADPOOL_H
