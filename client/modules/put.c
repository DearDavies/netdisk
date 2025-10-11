#include "put.h"

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
 * 发送单个数据帧：4B 长度（网络序）+ 数据体。
 */
static int send_frame(int fd, const void* buf, size_t len) {
    uint32_t net_len = htonl((uint32_t)len);
    if (send(fd, &net_len, sizeof(net_len), MSG_NOSIGNAL) == -1) return -1;
    if (len > 0) {
        if (send(fd, buf, len, MSG_NOSIGNAL) == -1) return -2;
    }
    return 0;
}

/*
 * 接收服务端的“长度前缀 + 文本”响应，分配并返回字符串（需调用方 free）。
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
 * 从形如 key1=val1&key2=val2 的返回中解析 key。
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

void modules_put(order_t instruction, user_t* user_status, int sock_fd) {
    /*
     * 支持格式：put <local_path> [remote_path]
     * - local_path 必填：本地文件路径
     * - remote_path 选填：服务器逻辑路径（可为目录或包含文件名的路径）；未给则使用本地文件名
     */
    if (!instruction.paras || strlen(instruction.paras) == 0) {
        printf("用法: put <local_path> [remote_path]\n");
        return;
    }

    // 解析 local_path 与 optional remote_path
    char* temp = strdup(instruction.paras);
    char* saveptr = NULL;
    char* token = strtok_r(temp, " ", &saveptr);
    char* local_path = token;
    char* remote_path = strtok_r(NULL, " ", &saveptr); // 可能为 NULL

    // 若未提供 remote_path，用本地文件名作为远端文件名
    char* filename = strrchr(local_path, '/');
    filename = filename ? filename + 1 : local_path;
    if (!remote_path) remote_path = filename;

    // 获取本地文件大小
    struct stat st;
    if (stat(local_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        printf("本地文件不存在或不可读: %s\n", local_path);
        free(temp);
        return;
    }

    // 组织头部 send_message_t：放入 username, pwd, paras(=remote_path), filename, size
    send_message_t send_message = {0};
    send_message.order_type = PUT;

    // 构造 kv 字符串
    char kv[4096];
    snprintf(kv, sizeof(kv), "username=%s&pwd=%s&paras=%s&filename=%s&size=%ld",
             user_status->username, user_status->my_pwd, remote_path, filename, (long)st.st_size);
    strncpy(send_message.paras, kv, sizeof(send_message.paras) - 1);

    // 发送头部
    if (send(sock_fd, &send_message, sizeof(send_message_t), MSG_NOSIGNAL) == -1) {
        LOG_ERROR("发送头部失败");
        free(temp);
        return;
    }

    // 等待服务端 ready 响应
    char* resp = recv_kv_response_alloc(sock_fd);
    if (!resp) {
        free(temp);
        return;
    }
    char result[64] = {0};
    parse_kv_local(resp, "result", result, sizeof(result));
    if (strcasecmp(result, "ok") != 0) {
        printf("服务端拒绝: %s\n", resp);
        free(resp);
        free(temp);
        return;
    }
    free(resp);

    // 发送数据分片
    int fd = open(local_path, O_RDONLY);
    if (fd < 0) {
        free(temp);
        return;
    }
    char buffer[8192];
    while (1) {
        ssize_t r = read(fd, buffer, sizeof(buffer));
        if (r < 0) {
            close(fd);
            free(temp);
            return;
        }
        if (r == 0) break;
        if (send_frame(sock_fd, buffer, (size_t)r) != 0) {
            close(fd);
            free(temp);
            return;
        }
    }
    close(fd);
    // 发送结束帧
    send_frame(sock_fd, NULL, 0);

    // 接收最终结果
    resp = recv_kv_response_alloc(sock_fd);
    if (!resp) {
        free(temp);
        return;
    }
    memset(result, 0, sizeof(result));
    parse_kv_local(resp, "result", result, sizeof(result));
    if (strcasecmp(result, "ok") == 0) {
        printf("上传完成: %s -> %s\n", local_path, remote_path);
    }
    else {
        printf("上传失败: %s\n", resp);
    }
    free(resp);
    free(temp);
}
