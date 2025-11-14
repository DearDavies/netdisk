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
#include <limits.h>

/*
 * 发送一个数据分片：4B 长度（网络序）+ 数据体。
 */
static int send_frame(int fd, const void* buf, size_t len) {
    // 先发送 4 字节的网络序长度，以便客户端知道后续需要读取多少数据。
    uint32_t net_len = htonl((uint32_t)len);
    if (send(fd, &net_len, sizeof(net_len), MSG_NOSIGNAL) <= 0) return -1;
    if (len > 0) {
        // 再发送真正的数据负载。
        if (send(fd, buf, len, MSG_NOSIGNAL) <= 0) return -2;
    }
    return 0;
}

void modules_get_handle(int client_fd,
                        const char* base_path,
                        const char* username,
                        const char* pwd,
                        const char* remote_path_kv,
                        db_handle_t* db) {
    if (!db || !db->conn) {
        send_kv_response(client_fd, "result=fail&error=database not ready");
        return;
    }
    if (!base_path || !username || !remote_path_kv || remote_path_kv[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=invalid request");
        return;
    }

    // 1) 解析目标文件逻辑路径。
    char logical_path[PATH_MAX] = {0};
    build_logical_path(pwd, remote_path_kv, logical_path, sizeof(logical_path));

    char parent_path[PATH_MAX] = {0};
    char display_name[256] = {0};
    split_parent_and_name(logical_path, parent_path, sizeof(parent_path), display_name, sizeof(display_name));
    if (display_name[0] == '\0') {
        // 无法拆分出合法的文件名，说明路径非法。
        send_kv_response(client_fd, "result=fail&error=invalid path");
        return;
    }

    // 2) 查询元数据，确认该逻辑路径存在且为文件。
    db_file_detail_t detail = {0};
    int rc = db_get_entry_by_path(db, username, parent_path, display_name, &detail);
    if (rc != 0) {
        // 记录不存在视为无法下载。
        send_kv_response(client_fd, "result=not a file");
        return;
    }
    if (detail.storage_name[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=storage missing");
        return;
    }

    char storage_path[PATH_MAX] = {0};
    snprintf(storage_path, sizeof(storage_path), "%s/%s", base_path, detail.storage_name);

    // 3) 打开并 stat 真实存储文件。
    struct stat st;
    if (stat(storage_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        send_kv_response(client_fd, "result=not a file");
        return;
    }
    char head[256] = {0};
    snprintf(head, sizeof(head), "result=ok&size=%ld", (long)st.st_size);
    send_kv_response(client_fd, head);

    int fd = open(storage_path, O_RDONLY);
    if (fd < 0) {
        // 如果文件突然无法打开，立即返回错误给客户端。
        send_kv_response(client_fd, "result=open failed");
        return;
    }

    // 4) 发送文件内容。
    char buffer[8192];
    while (1) {
        ssize_t r = read(fd, buffer, sizeof(buffer));
        if (r < 0) {
            // 读文件出错直接中断，客户端会在收不到帧后断开。
            close(fd);
            return;
        }
        if (r == 0) break;
        if (send_frame(client_fd, buffer, (size_t)r) != 0) { close(fd); return; }
    }
    close(fd);
    send_frame(client_fd, NULL, 0);
}
