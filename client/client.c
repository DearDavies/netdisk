/*
* 客户端：
 *      连接服务端
 *      接收服务端发来的“hello”消息即可
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include "client.h"
#include "logger.h"
#include "util.h"
#include "MACRO.h"
#include "read_config.h"
#include "subroutine.h"

#define BUFFERMAX 60

int main(int argc, char* argv[]) {
    // 初始化日志
    const char* config_file_name = "client_config.ini";
    if (log_init(config_file_name) != 0) {
        fprintf(stderr, "Logger 初始化失败\n");
        return 1;
    }

    atexit(log_cleanup);

    user_t user_status = {0};
    char buffer[BUFFERMAX] = {0};
    Section* config = parse_ini_file(config_file_name);
    const char* serverip = get_config_value(config, "server", "ip_address");
    LOG_DEBUG("从%s获取到服务器ip为%s", config_file_name, serverip);
    const char* port = get_config_value(config, "server", "port");
    LOG_DEBUG("从%s获取到服务器port为%s", config_file_name, port);
    free_config(config);

    // 与服务器建立连接
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(port));
    server.sin_addr.s_addr = inet_addr(serverip);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        LOG_ERROR("socket 初始化失败");
        exit(EXIT_FAILURE);
    }
    if (connect(sockfd, (struct sockaddr*)&server, sizeof(server)) == -1) {
        LOG_ERROR("connect 函数初始化失败");
        exit(EXIT_FAILURE);
    }


    //
    char* user_order = NULL;
    while (1) {
        // 如果没有登录
        if (check_login(user_status) == SIGNOUT) {
            int choice = 0;
            printf("1. 注册；2. 登录；3. 退出\n输入要执行的功能编号：");
            scanf("%d", &choice);
            // 清空输入缓冲区
            scanf("%*[^\n]");
            scanf("%*c");

            try_login(choice, &user_status, sockfd);
            if (user_status.exit_flag == EXIT_FLAG_YES) {
                close(sockfd);
                break;
            }
        }
        else {
            // 接收用户输入的命令
            read_user_order(&user_order, user_status);

            // 解析命令
            order_t instruction = {0};
            parse_user_order(user_order, &instruction, &user_status);

            // 根据不同的命令，执行不同流程
            dispath_order(instruction, &user_status, sockfd);
        }
    }
    return 0;
}
