#include "head.h"
#include "queue.h"
#include "threadpool.h"

#define BUFFERMAX 10
#define CLIENTS 4

int pipe_fd[2];

void caught_sigint(int sig) {
    printf("主进程已收到信号%d\n", sig);
    write(pipe_fd[1], "SIGINT", 6);
}

int main() {
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();

    // 父进程，只负责监听什么时候来信号，并写管道即可
    if (pid > 0) {
        signal(SIGINT, caught_sigint);
        wait(NULL);
        // printf("子进程已结束\n");
        exit(0);
    }

    // 子进程
    // 把子进程拉到后台进程组，不接收信号
    if (setpgid(0, 0) < 0) {
        perror("setpgid failed");
        exit(EXIT_FAILURE);
    }
    printf("子进程（pid = %d）进入后台进程组（pgid = %d）\n", getpid(), getpgid(getpid()));

    char* server_ip = "0.0.0.0";
    char* server_port = "8563";
    queue_t queue = {0};
    threadpool_t threadpool = {0};
    int socked_fd = 0;

    // 创建 epoll_fd
    int epoll_fd = epoll_create(1);
    if (epoll_fd == -1) {
        perror("创建 epoll_fd 失败");
        exit(EXIT_FAILURE);
    }

    // 初始化队列
    init_queue(&queue);

    // 初始化线程池
    init_threadpool(&threadpool, CLIENTS, &queue);

    // 初始化 socked_fd
    if (init_socket(&socked_fd, server_ip, server_port) != 0) {
        printf("初始化 socket_fd 出错\n");
        exit(EXIT_FAILURE);
    }

    // 将 socket_fd 添加到 epoll_fd 监听
    if (add_epoll(epoll_fd, socked_fd) != 0) {
        printf("添加 socket_fd 出错\n");
        exit(EXIT_FAILURE);
    }

    // 将 pipe_fd[0] 添加到 epoll_fd 监听
    if (add_epoll(epoll_fd, pipe_fd[0]) != 0) {
        printf("添加 pipe_fd[0] 出错\n");
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[2] = {0};

    while (1) {
        int ret_epoll = epoll_wait(epoll_fd, events, 2, -1);
        if (ret_epoll == -1) {
            printf("epoll_wait 返回的值为 -1\n");
            exit(EXIT_FAILURE);
        }
        if (ret_epoll == 0) {
            perror("epoll_wait 超时了");
        }
        else {
            for (int i = 0; i < ret_epoll; i++) {
                int event_fd = events[i].data.fd;

                // 如果是 socket_fd，说明来了新的客户端，加入到队列
                if (event_fd == socked_fd) {
                    int new_client_fd = accept(socked_fd, NULL, NULL);
                    if (new_client_fd == -1) {
                        perror("主线程获取客户端连接失败");
                        continue;
                    }
                    enqueue(&queue, new_client_fd);
                }
                // 如果 pipe_fd[0]来消息
                // 说明父进程收到了中断请求，子进程需要通知所有子线程终止
                else if (event_fd == pipe_fd[0]) {
                    char message[7] = {0};
                    read(pipe_fd[0], message, 7);

                    printf("正在向队列中添加 4 个终止消息符\n");
                    for (int i = 0; i < CLIENTS; i++) {
                        enqueue(&queue, -2);
                    }

                    printf("等待子进程的所有子线程退出\n");
                    for (int i = 0; i < CLIENTS; i++) {
                        pthread_join(threadpool.threads[i], NULL);
                    }

                    pthread_exit(NULL);
                }
            }
        }
    }


    return 0;
}
