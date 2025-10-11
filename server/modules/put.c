#include "put.h"
#include "common.h"

#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>

/*
 * 决定最终落盘文件路径：
 * - remote_path_kv 若指向目录，则使用该目录 + filename
 * - 否则 remote_path_kv 视为包含文件名的路径
 */
static void decide_target_path(const char* base_path,
                               const char* username,
                               const char* pwd,
                               const char* remote_path_kv,
                               const char* filename_kv,
                               char* out_abs,
                               size_t out_abs_sz) {
    char abs_candidate[4096] = {0};
    // 先将 remote_path_kv 规范化到真实路径（可能是“目录”或“文件”）
    normalize_join_path(base_path, username, pwd, remote_path_kv, abs_candidate, sizeof(abs_candidate), NULL, 0);
    if (is_dir(abs_candidate)) {
        // 若是目录，则拼接文件名
        if (abs_candidate[strlen(abs_candidate)-1] == '/') {
            snprintf(out_abs, out_abs_sz, "%s%s", abs_candidate, filename_kv);
        } else {
            snprintf(out_abs, out_abs_sz, "%s/%s", abs_candidate, filename_kv);
        }
    } else {
        // 否则 remote_path_kv 本身就是包含文件名的路径
        snprintf(out_abs, out_abs_sz, "%s", abs_candidate);
    }
}

/*
 * 从 socket 接收一个帧：4B 长度（网络序） + 数据体。
 * 返回接收的数据长度；len==0 表示结束帧；<0 表示错误/对端关闭。
 */
static ssize_t recv_frame(int fd, void* buf, size_t buf_sz) {
    uint32_t net_len = 0;
    ssize_t n = recv(fd, &net_len, sizeof(net_len), MSG_WAITALL);
    if (n <= 0) return -1;
    uint32_t len = ntohl(net_len);
    if (len == 0) return 0;
    if (len > buf_sz) return -2; // 防止溢出
    ssize_t m = recv(fd, buf, len, MSG_WAITALL);
    if (m <= 0) return -3;
    return (ssize_t)len;
}

void modules_put_handle(int client_fd, const char* base_path, const char* username, const char* pwd, const char* remote_path_kv, const char* filename_kv, const char* size_kv) {
    // 1) 计算目标文件绝对路径
    char target_path[4096] = {0};
    decide_target_path(base_path, username, pwd, remote_path_kv, filename_kv, target_path, sizeof(target_path));

    // 2) 准备写入：若上级目录不存在，返回错误（可扩展为递归创建）
    char dirbuf[4096];
    strncpy(dirbuf, target_path, sizeof(dirbuf)-1);
    char* last_slash = strrchr(dirbuf, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (!is_dir(dirbuf)) {
            send_kv_response(client_fd, "result=parent dir not exists");
            return;
        }
    }

    // 3) 发送 ready（result=ok）给客户端，使其开始传输
    send_kv_response(client_fd, "result=ok");

    // 4) 打开目标文件写入（覆盖写）
    int fd = open(target_path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (fd < 0) {
        send_kv_response(client_fd, "result=open failed");
        return;
    }

    // 5) 循环接收分片帧并写入到文件
    char buffer[8192];
    ssize_t rlen;
    long long total = 0;
    while (1) {
        rlen = recv_frame(client_fd, buffer, sizeof(buffer));
        if (rlen < 0) { // 出错或对端关闭
            close(fd);
            send_kv_response(client_fd, "result=recv failed");
            return;
        }
        if (rlen == 0) { // 结束帧
            break;
        }
        ssize_t written = 0;
        while (written < rlen) {
            ssize_t w = write(fd, buffer + written, (size_t)(rlen - written));
            if (w <= 0) {
                close(fd);
                send_kv_response(client_fd, "result=write failed");
                return;
            }
            written += w;
        }
        total += rlen;
    }
    close(fd);

    // 6) 回包最终结果
    send_kv_response(client_fd, "result=ok");
}


