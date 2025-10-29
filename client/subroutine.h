#ifndef NETDISK_SUBROUTINE_H
#define NETDISK_SUBROUTINE_H

#include "client.h"

// 执行登录、注册、退出的流程。
// sock_fd: 与服务端的连接套接字
void try_login(int choice, user_t* user_status, int sock_fd);

// 接收用户输入的命令
void read_user_order(char** user_order, user_t user_status);

// 解析命令
void parse_user_order(const char* user_order, order_t* instruction, user_t* user_status);

// 根据不同的命令，执行不同流程
void dispath_order(order_t instruction, user_t* user_status, int sock_fd);

#endif //NETDISK_SUBROUTINE_H
