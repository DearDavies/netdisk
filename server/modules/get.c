#include "get.h"
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
 * 发送一个数据分片：4B 长度（网络序）+ 数据体。
 */
static int send_frame(int fd, const void* buf, size_t len) {
    uint32_t net_len = htonl((uint32_t)len);
    if (send(fd, &net_len, sizeof(net_len), MSG_NOSIGNAL) <= 0) return -1;
    if (len > 0) {
        if (send(fd, buf, len, MSG_NOSIGNAL) <= 0) return -2;
    }
    return 0;
}

void modules_get_handle(int client_fd, const char* base_path, const char* username, const char* pwd, const char* remote_path_kv) {
    // 1) 解析目标文件绝对路径
    char abs_path[4096] = {0};
    normalize_join_path(base_path, username, pwd, remote_path_kv, abs_path, sizeof(abs_path), NULL, 0);

    // 2) 打开并 stat 文件
    struct stat st;
    if (stat(abs_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        send_kv_response(client_fd, "result=not a file");
        return;
    }
    char head[256] = {0};
    snprintf(head, sizeof(head), "result=ok&size=%ld", (long)st.st_size);
    send_kv_response(client_fd, head);

    // 3) 发送分片数据
    int fd = open(abs_path, O_RDONLY);
    if (fd < 0) {
        send_kv_response(client_fd, "result=open failed");
        return;
    }
    char buffer[8192];
    while (1) {
        ssize_t r = read(fd, buffer, sizeof(buffer));
        if (r < 0) { close(fd); return; }
        if (r == 0) break;
        if (send_frame(client_fd, buffer, (size_t)r) != 0) { close(fd); return; }
    }
    close(fd);
    // 4) 发送结束帧
    send_frame(client_fd, NULL, 0);
}


