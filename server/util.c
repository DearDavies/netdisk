#include "head.h"

// 初始化 socket_fd，失败返回非零值
int init_socket(int* socket_fd, char* ip, char* port) {
    struct sockaddr_in local_server = {0};
    local_server.sin_family = AF_INET;
    local_server.sin_port = htons(atoi(port));
    local_server.sin_addr.s_addr = inet_addr(ip);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("init_socket：socket");
        return -1;
    }
    // 设置端口重用
    int reuseaddr = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int));
    // 绑定
    if (bind(sockfd, (struct sockaddr*)&local_server, sizeof(struct sockaddr)) == -1) {
        perror("init_socket：bind");
        return -2;
    }
    // 监听
    if (listen(sockfd, 10) == -1) {
        perror("init_socket：listen");
        return -3;
    }
    *socket_fd = sockfd;
    return 0;
}

// 为 epoll_fd 添加读的监听事件，失败返回非零值
int add_epoll(int epoll_fd, int event_fd) {
    if (epoll_fd < 0) {
        perror("epoll_fd 无效");
        return -1;
    }
    if (event_fd < 0 ) {
        perror("要监听的读事件无效");
        return -2;
    }
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = event_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &event);
    return 0;
}