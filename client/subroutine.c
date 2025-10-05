//
// Created by deda on 2025/10/4.
//
#include "subroutine.h"
#include "util.h"
#include "MACRO.h"
#include "logger.h"
#include "read_config.h"
#include "client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "modules/cd.h"
#include "modules/get.h"
#include "modules/ls.h"
#include "modules/mkdir.h"
#include "modules/put.h"
#include "modules/pwd.h"
#include "modules//rm.h"

// 执行登录、注册、退出的流程。
void try_login(int choice, user_t* user_status) {
    if (choice == REGISTER) {
        // 执行注册流程，打印提示
        char reg_username[100] = {0};
        char reg_password[128] = {0};
        char reg_repeat_password[128] = {0};
        // 接收输入的用户名、密码
        printf("请输入用户名：");
        fgets(reg_username, sizeof(reg_username) - 1, stdin);
        for (int i = 0; i < 3; i++) {
            printf("请输入密码（不会回显）：");
            fflush(stdout);
            get_password(reg_password, sizeof(reg_password));
            printf("请再输入密码（不会回显）：");
            fflush(stdout);
            get_password(reg_repeat_password, sizeof(reg_repeat_password));
            if (strcmp(reg_password, reg_repeat_password) == 0) {
                break;
            }
            printf("两次输入的密码不一致！请重新输入密码（%d/3）\n", i + 1);
            if (i == 2) {
                printf("失败次数过多！将重试\n");
                return;
            }
        }


        // 将信息组织好、并发送到服务器

        // 如果服务器回复成功
        if (1) {
            // 打印注册成功
            // 执行登录流程
            choice = LOGIN;
        }
        else {
            // 提示注册失败，并重新回到初始流程
            return;
        }
    }
    if (choice == LOGIN) {
        char login_username[100] = {0};
        char login_password[128] = {0};
        // 接收输入的用户名、密码
        printf("请输入用户名：");
        fgets(login_username, sizeof(login_username) - 1, stdin);

        printf("请输入密码（不会回显）：");
        fflush(stdout);
        get_password(login_password, sizeof(login_password));
        // 将信息组织好、并发送到服务器

        // 如果服务器回复成功
        if (1) {
            printf("登录成功。\n");
            // 修改存储状态
            user_status->is_login = SIGNIN;
            user_status->exit_flag = EXIT_FLAG_NO;
            user_status->username = strdup(login_username);
            user_status->my_pwd = strdup("/");
        }
        else {
            // 提示注册失败，并重新回到初始流程
            printf("输入的信息有误！\n");
            return;
        }
    }
    if (choice == EXIT) {
        printf("谢谢使用，再见。\n");
        user_status->exit_flag = EXIT_FLAG_YES;
    }
}

// 接收用户输入的命令
void read_user_order(char** user_order, user_t user_status) {
    if (user_order == NULL) {
        LOG_ERROR("user_order 的地址是空指针");
        return;
    }

    char buffer[1024];
    printf("%s:%s$ ", user_status.username, user_status.my_pwd);
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        LOG_ERROR("从标准输入读取用户命令错误");
        return;
    }

    // 删除指令最后的换行
    buffer[strcspn(buffer, "\n")] = '\0';

    *user_order = strdup(buffer);

    if (*user_order == NULL) {
        LOG_ERROR("strdup 分配内存错误");
    }
}

// 解析命令
void parse_user_order(const char* user_order, order_t* instruction, user_t* user_status) {
    // 临时存储一份用户指令
    char* user_order_temp = strdup(user_order);
    // 去除前后的空白字符
    char* trimmed_order = trim_whitespace(user_order_temp);
    // 如果用户输入“exit”（不区分大小写），则退出当前用户的登录状态
    if (strcasecmp(trimmed_order, "exit") == 0) {
        user_status->exit_flag = EXIT_FLAG_NO;
        user_status->is_login = SIGNOUT;
        // 获取用户名、释放空间、置none
        char* temp = user_status->username;
        free(temp);
        user_status->username = "none";
        // 获取pwd、释放空间、置初始
        temp = user_status->my_pwd;
        free(temp);
        user_status->my_pwd = "/";
        return;
    }
    // 从前往后，找到第一个空格的索引下标
    size_t index = strcspn(trimmed_order, " ");
    // 将空格置为0
    trimmed_order[index] = '\0';
    // 获取指令类型
    char* order_type = strdup(trimmed_order);

    if (strcasecmp(order_type, "ls") == 0) {
        instruction->order_type = LS;
        instruction->paras = NULL;
    }
    else if (strcasecmp(order_type, "pwd") == 0) {
        instruction->order_type = PWD;
        instruction->paras = NULL;
    }
    else if (strcasecmp(order_type, "cd") == 0) {
        instruction->order_type = CD;
        trimmed_order += index + 1;
        instruction->paras = strdup(trimmed_order);
    }
    else if (strcasecmp(order_type, "mkdir") == 0) {
        instruction->order_type = MKDIR;
        trimmed_order += (index + 1);
        instruction->paras = strdup(trimmed_order);
    }
    else if (strcasecmp(order_type, "put") == 0) {
        instruction->order_type = PUT;
        trimmed_order += index + 1;
        instruction->paras = strdup(trimmed_order);
    }
    else if (strcasecmp(order_type, "get") == 0) {
        instruction->order_type = GET;
        trimmed_order += index + 1;
        instruction->paras = strdup(trimmed_order);
    }
    else if (strcasecmp(order_type, "rm") == 0) {
        instruction->order_type = RM;
        trimmed_order += index + 1;
        instruction->paras = strdup(trimmed_order);
    }
    else {
        instruction->order_type = INVALID;
        instruction->paras = NULL;
    }
    LOG_DEBUG("解析用户输入的命令(%s)，种类是 %s，参数是 %s", user_order, order_type, instruction->paras);
    free(user_order_temp);
}

// 根据不同的命令，执行不同流程
void dispath(order_t instruction, user_t* user_status, int sock_fd) {
    if (user_status->is_login == SIGNOUT) {
        return;
    }
    switch (instruction.order_type) {
    case CD:
        modules_cd(instruction, user_status, sock_fd);
        break;
    case PWD:
        modules_pwd(instruction, user_status, sock_fd);
        break;
    case LS:
        modules_ls(instruction, user_status, sock_fd);
        break;
    case MKDIR:
        modules_mkdir(instruction, user_status, sock_fd);
        break;
    case PUT:
        modules_put(instruction, user_status, sock_fd);
        break;
    case GET:
        modules_get(instruction, user_status, sock_fd);
        break;;
    case RM:
        modules_rm(instruction, user_status, sock_fd);
        break;
    default:
        printf("无效命令！\n");
        break;
    }
}
