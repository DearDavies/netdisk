#include "ls.h"

#include <stddef.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

#include "../logger.h"
#include "../util.h"

void modules_ls(order_t instruction, user_t* user_status, int sock_fd) {
    char* final_result = NULL;
    int order_type = instruction.order_type;
    char* username = user_status->username;
    char* pwd = user_status->my_pwd;

    send_message_t send_message = {0};
    send_message.order_type = order_type;
    // 计算所需总长度
    // 格式: "username=" + username + "&pwd=" + pwd + "\0"
    size_t len = strlen("username=") + strlen(username) +
        strlen("&pwd=") + strlen(pwd) + 1; // +1 是为最后的空字符 '\0'

    // 动态分配内存
    char* result_string = (char*)calloc(len, sizeof(char));
    if (result_string == NULL) {
        // 内存分配失败，需要处理错误
        LOG_ERROR("发送参数时，分配内存错误");
        return;
    }

    // 使用 snprintf 安全地格式化拼接字符串
    snprintf(result_string, len, "username=%s&pwd=%s", username, pwd);

    LOG_DEBUG("拼接后的字符串: %s\n", result_string);
    strncpy(send_message.paras, result_string, strlen(result_string));
    // 将需要的指令，整合后发送到服务端
    if (send(sock_fd, &send_message, sizeof(send_message_t), MSG_NOSIGNAL) == -1) {
        LOG_ERROR("发送到服务器失败");
        return;
    }
    free(result_string);

    // 开始接收服务端
    size_t net_recv_len = 0;
    if (recv(sock_fd, &net_recv_len, sizeof(net_recv_len), MSG_WAITALL) == 0) {
        printf("服务端断开\n");
        LOG_INFO("服务端断开");
        return;
    }
    size_t recv_len = ntohl(net_recv_len);
    result_string = (char*)calloc(recv_len + 1, sizeof(char));
    if (result_string == NULL) {
        // 内存分配失败，需要处理错误
        LOG_ERROR("接收服务端返回结果时，分配内存错误");
        exit(EXIT_FAILURE);
    }
    if (recv(sock_fd, result_string, recv_len, MSG_WAITALL) == 0) {
        printf("服务端断开\n");
        LOG_ERROR("服务端断开");
        exit(EXIT_FAILURE);
    }
    const char* p = result_string; // 主遍历指针

    // 主循环，直到字符串末尾
    while (*p != '\0') {
        // 寻找 Key
        const char* key_start = p;
        while (*p != '\0' && *p != '=' && *p != '&') {
            p++;
        }
        const char* key_end = p; // key_end 指向 '=' 或 '\0'
        size_t key_len = key_end - key_start;

        // 如果 key 长度为0 (例如 "&&" 或开头是 "&")，则跳过
        if (key_len == 0) {
            if (*p == '&') {
                p++; // 跳过 '&'
            }
            continue;
        }

        // 寻找 Value
        const char* value_start = NULL;
        const char* value_end = NULL;
        size_t value_len = 0;

        if (*p == '=') {
            p++; // 跳过 '='
            value_start = p;
            while (*p != '\0' && *p != '&') {
                p++;
            }
            value_end = p; // value_end 指向 '&' 或 '\0'
            value_len = value_end - value_start;
        }

        // 判断 Key 并进行赋值
        if (key_len == strlen("result") && strncmp(key_start, "result", key_len) == 0) {
            if (value_len > 0) {
                final_result = (char*)calloc(value_len + 1, sizeof(char));
                strncpy(final_result, value_start, value_len);
                LOG_INFO("ls命令的执行效果为：%s", final_result);
            }
        }

        // 如果不是字符串末尾，前进到下一对
        if (*p == '&') {
            p++;
        }
    }
    printf("%s\n", final_result);
    printf("%s:%s$ ", user_status->username, user_status->my_pwd);
    free(result_string);
    free(final_result);
}
