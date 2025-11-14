#include "rm.h"

#include <stddef.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <limits.h>

#include "../logger.h"
#include "../util.h"

void modules_rm(order_t instruction, user_t* user_status, int sock_fd) {
    if (!instruction.paras || instruction.paras[0] == '\0') {
        printf("用法: rm <path>\n");
        return;
    }

    const char* username = user_status->username;
    const char* pwd = user_status->my_pwd;
    const char* target = instruction.paras;
    char confirm_flag[4] = "no"; // confirm=yes 时才会强制删除非空目录
    int finished = 0;

    while (!finished) {
        send_message_t send_message = {0};
        send_message.order_type = instruction.order_type;

        // payload 中包含 confirm 标志，服务端通过 confirm=yes 判断用户是否已确认删除。
        size_t len = strlen("username=") + strlen(username) +
                     strlen("&pwd=") + strlen(pwd) +
                     strlen("&paras=") + strlen(target) +
                     strlen("&confirm=") + strlen(confirm_flag) + 1;
        char* payload = (char*)calloc(len, sizeof(char));
        if (!payload) {
            LOG_ERROR("rm 命令：拼接参数内存分配失败");
            return;
        }
        snprintf(payload, len, "username=%s&pwd=%s&paras=%s&confirm=%s",
                 username, pwd, target, confirm_flag);
        strncpy(send_message.paras, payload, sizeof(send_message.paras) - 1);

        if (send(sock_fd, &send_message, sizeof(send_message_t), MSG_NOSIGNAL) == -1) {
            LOG_ERROR("rm 命令发送失败");
            free(payload);
            return;
        }
        free(payload);

        uint32_t net_recv_len = 0;
        if (recv(sock_fd, &net_recv_len, sizeof(net_recv_len), MSG_WAITALL) == 0) {
            printf("服务端断开\n");
            LOG_INFO("服务端断开");
            return;
        }
        uint32_t recv_len = ntohl(net_recv_len);
        char* result_string = (char*)calloc(recv_len + 1, sizeof(char));
        if (!result_string) {
            LOG_ERROR("rm 命令：读取结果内存分配失败");
            return;
        }
        if (recv(sock_fd, result_string, recv_len, MSG_WAITALL) == 0) {
            printf("服务端断开\n");
            LOG_ERROR("服务端断开");
            free(result_string);
            return;
        }

        char result_buf[64] = {0};
        char message_buf[512] = {0};
        char error_buf[512] = {0};
        char path_buf[PATH_MAX] = {0};

        const char* p = result_string;
        while (*p != '\0') {
            const char* key_start = p;
            while (*p != '\0' && *p != '=' && *p != '&') p++;
            size_t key_len = (size_t)(p - key_start);
            if (key_len == 0) {
                if (*p == '&') p++;
                continue;
            }
            const char* value_start = NULL;
            const char* value_end = NULL;
            size_t value_len = 0;
            if (*p == '=') {
                p++;
                value_start = p;
                while (*p != '\0' && *p != '&') p++;
                value_end = p;
                value_len = (size_t)(value_end - value_start);
            }
            if (*p == '&') p++;

            if (value_len == 0) continue;
            if (key_len == strlen("result") && strncmp(key_start, "result", key_len) == 0) {
                size_t copy_len = value_len < sizeof(result_buf) - 1 ? value_len : sizeof(result_buf) - 1;
                memcpy(result_buf, value_start, copy_len);
                result_buf[copy_len] = '\0';
            } else if (key_len == strlen("message") && strncmp(key_start, "message", key_len) == 0) {
                size_t copy_len = value_len < sizeof(message_buf) - 1 ? value_len : sizeof(message_buf) - 1;
                memcpy(message_buf, value_start, copy_len);
                message_buf[copy_len] = '\0';
            } else if (key_len == strlen("error") && strncmp(key_start, "error", key_len) == 0) {
                size_t copy_len = value_len < sizeof(error_buf) - 1 ? value_len : sizeof(error_buf) - 1;
                memcpy(error_buf, value_start, copy_len);
                error_buf[copy_len] = '\0';
            } else if (key_len == strlen("path") && strncmp(key_start, "path", key_len) == 0) {
                size_t copy_len = value_len < sizeof(path_buf) - 1 ? value_len : sizeof(path_buf) - 1;
                memcpy(path_buf, value_start, copy_len);
                path_buf[copy_len] = '\0';
            }
        }
        free(result_string);

        if (strcasecmp(result_buf, "need_confirm") == 0) {
            if (strcasecmp(confirm_flag, "yes") == 0) {
                printf("删除失败：目录仍被判定为非空\n");
                break;
            }
            const char* prompt_path = path_buf[0] ? path_buf : target;
            const char* prompt_msg = message_buf[0] ? message_buf : "目录非空，确认删除？";
            printf("%s (%s) [y/N]: ", prompt_msg, prompt_path);
            fflush(stdout);
            char answer[8] = {0};
            if (!fgets(answer, sizeof(answer), stdin)) {
                printf("已取消删除。\n");
                break;
            }
            if (answer[0] == '\n') {
                printf("已取消删除。\n");
                break;
            }
            if (tolower((unsigned char)answer[0]) == 'y') {
                // 用户确认后设置 confirm=yes，下一轮循环会立即重发同一请求。
                strcpy(confirm_flag, "yes");
                continue; // 重新发送，附带 confirm=yes
            }
            printf("已取消删除。\n");
            break;
        } else if (strcasecmp(result_buf, "ok") == 0) {
            printf("删除完成: %s\n", target);
            break;
        } else {
            const char* detail = error_buf[0] ? error_buf :
                                 (message_buf[0] ? message_buf : "删除文件（夹）失败");
            printf("%s\n", detail);
            break;
        }
    }

    printf("%s:%s$ ", user_status->username, user_status->my_pwd);
}
