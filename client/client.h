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
    // 是否登录
    int is_login;
    // 用户是否选择了退出程序
    int exit_flag;
    // 用户名
    char* username;
    // 当前工作目录
    char* my_pwd;
} user_t;

#endif //NETDISK_CLIENT_H
