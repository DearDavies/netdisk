#include "work.h"
#include "logger.h"

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFERMAX 1024

int do_work(int client_fd, const char* base_path) {
    char* recv_buffer[BUFFERMAX] = {0};

    send_message_t recv_send_message = {0};
    while (1) {
        if (recv(client_fd, &recv_send_message, sizeof(recv_send_message), MSG_WAITALL) == 0) {
            LOG_INFO("客户端断开");
            return -1;
        }
        switch (recv_send_message.order_type) {
            case CD:
                break;
            case LS:
                break;
            case MKDIR:
                break;
            case RM:
                break;
            default:
                LOG_INFO("客户端发来一个无效命令");
        }
    }


    if (send(client_fd, "hello", 5, MSG_NOSIGNAL) == -1) {
        return -1;
    }
    close(client_fd);
    return 0;
}
