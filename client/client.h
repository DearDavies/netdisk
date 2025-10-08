#ifndef NETDISK_CLIENT_H
#define NETDISK_CLIENT_H

/*
 * 解析用户的命令:
 *      分为两部分：
 *          1. 命令类别(int)
 *          2. 参数(char paras[1024])
 */

typedef enum {
    INVALID,
    EMPTY,
    CD,
    MKDIR,
    PUT,
    GET,
    LS,
    RM,
    PWD
} order_type_t;

typedef struct {
    order_type_t order_type;
    char* paras;
} order_t;

typedef struct {
    int is_login; // 是否登录
    int exit_flag; // 用户是否选择了退出程序
    char* username; // 用户名
    char* my_pwd; // 当前工作目录
} user_t;

typedef struct {
    int order_type;
    char paras[4097];
} send_message_t;

#endif //NETDISK_CLIENT_H
