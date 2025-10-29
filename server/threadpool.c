#include "threadpool.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <error.h>
#include <sys/stat.h>
#include "logger.h"
#include "work.h"
#include "read_config.h"

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
    const char* config_file_name = "server_config.ini";
    queue_t* queue = (queue_t*)arg;
    pthread_t tid = pthread_self();
    LOG_INFO("子线程（%ld）已启动", tid);
    while (1) {
        int new_client_fd = dequeue(queue);

        // 如果收到了 -2，表示主线程传达了退出消息。
        if (new_client_fd == -2) {
            LOG_INFO("子线程（%ld）接收到了退出信号，即将退出", tid);
            // 子线程在退出前，务必要先检查有无重要资源，例如锁。
            // 如果有锁，先释放锁，可以避免其他等待的子线程出现死锁；
            // 或者注册 pthread_cleanup 函数，用来释放锁。
            // 由于本代码中，把共享资源的上锁、解锁解耦在队列操作，因此本代码不需要解锁
            pthread_exit(NULL);
        }
        LOG_INFO("子线程（%ld）正在工作，获取到了连接%d", tid, new_client_fd);
        Section* config = parse_ini_file(config_file_name);
        const char* base_path = get_config_value(config, "server", "base_path");
        LOG_DEBUG("服务端成功获取到base_path = %s", base_path);
        int ret = do_work(new_client_fd, base_path);
        if (ret != 0) {
            if (ret == -1) {
                LOG_INFO("客户端断开连接");
            }
            else {
                LOG_INFO("子线程执行 do_work 出错了，返回码是：%d", ret);
            }
        }
    }
}
