#ifndef HEAD_H
#define HEAD_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <error.h>
#include <sys/stat.h>
#include <sys/wait.h>

// 初始化 socket_fd，失败返回非零值
int init_socket(int* socket_fd, char* ip, char* port);

// 为 epoll_fd 添加读的监听事件，失败返回非零值
int add_epoll(int epoll_fd, int event_fd);

#endif //HEAD_H
