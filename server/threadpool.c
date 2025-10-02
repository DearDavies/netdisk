#include "threadpool.h"

#include <stdio.h>
#include <stdlib.h>

void init_threadpool(threadpool_t* threadpool, int num, queue_t* queue) {
    threadpool->num = num;
    threadpool->threads = (pthread_t*)calloc(num, sizeof(pthread_t));
    for (int i = 0; i < num; i++) {
        pthread_create(threadpool->threads + i, NULL, thread_main, (void*)queue);
    }
}

void destroy_threadpool(threadpool_t* threadpool) {
    free(threadpool->threads);
}

void* thread_main(void* arg) {
    queue_t* queue = (queue_t*)arg;
    pthread_t tid = pthread_self();
    printf("子线程（%ld）已启动\n", tid);
    while (1) {
        int new_client_fd = dequeue(queue);

        // 如果收到了 -2，表示主线程传达了退出消息。
        if (new_client_fd == -2) {
            printf("子线程（%ld）接收到了退出信号，即将退出\n", tid);
            // 子线程在退出前，务必要先检查有无重要资源，例如锁。
            // 如果有锁，先释放锁，可以避免其他等待的子线程出现死锁；
            // 或者注册 pthread_cleanup 函数，用来释放锁。
            // 由于本代码中，把共享资源的上锁、解锁解耦在队列操作，因此本代码不需要解锁
            pthread_exit(NULL);
        }
        printf("子线程（%ld）正在工作，获取到了连接 %d \n", tid, new_client_fd);
        int ret = do_work(new_client_fd);
        if (ret != 0) {
            if (ret == -1) {
                printf("客户端断开连接\n");
            }else {
                printf("子线程执行 do_work 出错了，返回码是：%d\n", ret);
            }
        }
    }
}

int do_work(int client_fd) {
    if (send(client_fd, "hello", 5, MSG_NOSIGNAL) == -1) {
        return -1;
    }
    close(client_fd);
    return 0;
}
