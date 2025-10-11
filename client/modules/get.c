#include "get.h"

#include <stddef.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../logger.h"
#include "../util.h"

/*
 * 接收服务端的“长度前缀 + 文本”响应，返回 malloc 的字符串。
 */
static char* recv_kv_response_alloc(int sock_fd) {
    uint32_t net_len = 0;
    if (recv(sock_fd, &net_len, sizeof(net_len), MSG_WAITALL) == 0) return NULL;
    uint32_t len = ntohl(net_len);
    char* s = (char*)calloc(len + 1, sizeof(char));
    if (!s) return NULL;
    if (len > 0) {
        if (recv(sock_fd, s, len, MSG_WAITALL) == 0) {
            free(s);
            return NULL;
        }
    }
    return s;
}

/*
 * 解析 key=value&... 的响应，取出 key。
 */
static int parse_kv_local(const char* kv, const char* key, char* out, size_t out_sz) {
    size_t key_len = strlen(key);
    const char* p = kv;
    while (p && *p) {
        const char* kstart = p;
        while (*p && *p != '=' && *p != '&') p++;
        const char* kend = p;
        if ((size_t)(kend - kstart) == key_len && strncmp(kstart, key, key_len) == 0 && *p == '=') {
            p++;
            const char* vstart = p;
            while (*p && *p != '&') p++;
            size_t vlen = (size_t)(p - vstart);
            if (vlen >= out_sz) vlen = out_sz - 1;
            memcpy(out, vstart, vlen);
            out[vlen] = '\0';
            return 0;
        }
        while (*p && *p != '&') p++;
        if (*p == '&') p++;
    }
    if (out_sz > 0) out[0] = '\0';
    return -1;
}

/*
 * 接收一帧：4B 长度 + 数据体。返回数据长度；0 表示结束；<0 错误。
 */
static ssize_t recv_frame(int fd, void* buf, size_t buf_sz) {
    uint32_t net_len = 0;
    ssize_t n = recv(fd, &net_len, sizeof(net_len), MSG_WAITALL);
    if (n <= 0) return -1;
    uint32_t len = ntohl(net_len);
    if (len == 0) return 0;
    if (len > buf_sz) return -2;
    ssize_t m = recv(fd, buf, len, MSG_WAITALL);
    if (m <= 0) return -3;
    return (ssize_t)len;
}

void modules_get(order_t instruction, user_t* user_status, int sock_fd) {
    /*
     * 支持格式：get <remote_path> [local_path]
     * 若未提供 local_path，使用 remote_path 的文件名保存到当前目录。
     */
    if (!instruction.paras || strlen(instruction.paras) == 0) {
        printf("用法: get <remote_path> [local_path]\n");
        return;
    }

    // 解析 remote_path 与 optional local_path
    char* temp = strdup(instruction.paras);
    char* saveptr = NULL;
    char* remote_path = strtok_r(temp, " ", &saveptr);
    char* local_path = strtok_r(NULL, " ", &saveptr);

    // 未提供 local_path 则取 remote_path 的文件名
    if (!local_path) {
        char* base = strrchr(remote_path, '/');
        base = base ? base + 1 : remote_path;
        local_path = base;
    }

    // 组织并发送头部（send_message_t）
    send_message_t send_message = {0};
    send_message.order_type = GET;
    char kv[4096];
    snprintf(kv, sizeof(kv), "username=%s&pwd=%s&paras=%s",
             user_status->username, user_status->my_pwd, remote_path);
    strncpy(send_message.paras, kv, sizeof(send_message.paras) - 1);
    if (send(sock_fd, &send_message, sizeof(send_message_t), MSG_NOSIGNAL) == -1) {
        LOG_ERROR("发送头部失败");
        free(temp);
        return;
    }

    // 接收服务端响应，获取 size
    char* resp = recv_kv_response_alloc(sock_fd);
    if (!resp) {
        free(temp);
        return;
    }
    char result[32] = {0};
    parse_kv_local(resp, "result", result, sizeof(result));
    if (strcasecmp(result, "ok") != 0) {
        printf("服务端错误: %s\n", resp);
        free(resp);
        free(temp);
        return;
    }
    char size_str[64] = {0};
    parse_kv_local(resp, "size", size_str, sizeof(size_str));
    long long total = atoll(size_str);
    free(resp);

    // 打开本地文件用于写入（覆盖）
    int fd = open(local_path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (fd < 0) {
        free(temp);
        return;
    }

    // 循环接收数据帧
    char buffer[8192];
    long long received = 0;
    while (1) {
        ssize_t rlen = recv_frame(sock_fd, buffer, sizeof(buffer));
        if (rlen < 0) {
            close(fd);
            free(temp);
            return;
        }
        if (rlen == 0) break;
        ssize_t written = 0;
        while (written < rlen) {
            ssize_t w = write(fd, buffer + written, (size_t)(rlen - written));
            if (w <= 0) {
                close(fd);
                free(temp);
                return;
            }
            written += w;
        }
        received += rlen;
    }
    close(fd);

    printf("下载完成: %s (%lld bytes) -> %s\n", remote_path, total, local_path);
    free(temp);
}
