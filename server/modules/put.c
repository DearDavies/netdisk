#include "put.h"
#include "common.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdint.h>
#include <sys/time.h>
#include <limits.h>

/*
 * 从 socket 接收一个帧：4B 长度（网络序） + 数据体。
 * 返回接收的数据长度；len==0 表示结束帧；<0 表示错误/对端关闭。
 */
static ssize_t recv_frame(int fd, void* buf, size_t buf_sz) {
    // 由于客户端按“长度前缀 + 数据体”的格式发送数据，这里先接收 4 字节长度。
    uint32_t net_len = 0;
    ssize_t n = recv(fd, &net_len, sizeof(net_len), MSG_WAITALL);
    if (n <= 0) return -1;
    uint32_t len = ntohl(net_len);
    // 长度为 0 代表对端发送结束帧，直接返回 0。
    if (len == 0) return 0;
    // 如果帧长度超过本地缓冲区，说明协议异常，直接报错。
    if (len > buf_sz) return -2; // 防止溢出
    ssize_t m = recv(fd, buf, len, MSG_WAITALL);
    if (m <= 0) return -3;
    return (ssize_t)len;
}

/*
 * 生成一个随机存储文件名，所有真实文件都统一写入 base_path 下。
 */
static void generate_storage_name(char* out, size_t out_sz) {
    // 使用时间戳 + 随机数 + 进程号拼接，降低文件名碰撞的概率。
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long us = (unsigned long long)tv.tv_sec * 1000000ULL + (unsigned long long)tv.tv_usec;
    unsigned long rnd = (unsigned long)rand();
    pid_t pid = getpid();
    snprintf(out, out_sz, "%llx_%lx_%x.dat", us, rnd, (unsigned int)pid);
}

/*
 * 删除旧的存储文件，避免冗余占用空间（仅在无人引用时执行）。
 */
static void remove_storage_file_if_unused(db_handle_t* db,
                                          const char* base_path,
                                          const char* old_storage_name,
                                          const char* new_storage_name) {
    if (!old_storage_name || old_storage_name[0] == '\0') return;
    if (new_storage_name && strcmp(old_storage_name, new_storage_name) == 0) return;

    int ref_count = 0;
    if (db_storage_refcount(db, old_storage_name, &ref_count) != 0) {
        return;
    }
    if (ref_count != 0) {
        return;
    }
    char path[PATH_MAX] = {0};
    snprintf(path, sizeof(path), "%s/%s", base_path, old_storage_name);
    unlink(path);
}

void modules_put_handle(int client_fd,
                        const char* base_path,
                        const char* username,
                        const char* pwd,
                        const char* remote_path_kv,
                        const char* filename_kv,
                        const char* size_kv,
                        const char* hash_kv,
                        db_handle_t* db) {
    if (!db || !db->conn) {
        send_kv_response(client_fd, "result=fail&error=database not ready");
        return;
    }
    if (!base_path || base_path[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=invalid base path");
        return;
    }
    if (!username || !filename_kv || filename_kv[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=invalid filename");
        return;
    }

    long long expect_total = size_kv ? atoll(size_kv) : 0;

    // 1) 计算上传目标的逻辑路径（完全基于 pwd 与 remote_path 组合）。
    char logical_path[PATH_MAX] = {0};
    if (!remote_path_kv || remote_path_kv[0] == '\0') {
        // 如果用户没有显式指定远端路径，则默认将文件直接放在当前工作目录下。
        build_logical_path(pwd, filename_kv, logical_path, sizeof(logical_path));
    } else {
        char normalized_remote[PATH_MAX] = {0};
        build_logical_path(pwd, remote_path_kv, normalized_remote, sizeof(normalized_remote));

        int treat_as_dir = (remote_path_kv[strlen(remote_path_kv) - 1] == '/');
        if (!treat_as_dir) {
            if (strcmp(normalized_remote, "/") == 0) {
                treat_as_dir = 1;
            } else {
                int dir_state = db_directory_exists(db, username, normalized_remote);
                if (dir_state == -1) {
                    send_kv_response(client_fd, "result=fail&error=dir lookup failed");
                    return;
                }
                if (dir_state == 0) {
                    treat_as_dir = 1;
                }
            }
        }
        if (treat_as_dir) {
            // 如果 remote_path 是目录，就把 filename 追加到该目录后面。
            build_logical_path(normalized_remote, filename_kv, logical_path, sizeof(logical_path));
        } else {
            strncpy(logical_path, normalized_remote, sizeof(logical_path) - 1);
            logical_path[sizeof(logical_path) - 1] = '\0';
        }
    }

    // 2) 分离父目录与文件名，并确认父目录逻辑上存在。
    char parent_path[PATH_MAX] = {0};
    char display_name[256] = {0};
    split_parent_and_name(logical_path, parent_path, sizeof(parent_path), display_name, sizeof(display_name));
    if (display_name[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=invalid target path");
        return;
    }
    int parent_state = db_directory_exists(db, username, parent_path);
    if (parent_state == -1) {
        send_kv_response(client_fd, "result=fail&error=dir lookup failed");
        return;
    }
    if (parent_state == 1) {
        // 父目录在数据库中不存在，无法写入。
        send_kv_response(client_fd, "result=parent dir not exists");
        return;
    }

    // 3) 查询是否已有同名文件，若已存在则记录旧 storage 以便覆盖后清理。
    db_file_detail_t existing_detail = {0};
    int existing_rc = db_get_entry_by_path(db, username, parent_path, display_name, &existing_detail);
    char old_storage_name[256] = {0};
    if (existing_rc == -1) {
        // 数据库查询失败直接返回。
        send_kv_response(client_fd, "result=fail&error=metadata query failed");
        return;
    }
    if (existing_rc == 0 && existing_detail.storage_name[0] != '\0') {
        strncpy(old_storage_name, existing_detail.storage_name, sizeof(old_storage_name) - 1);
        old_storage_name[sizeof(old_storage_name) - 1] = '\0';
    }

    // 4) 若客户端上传了文件哈希，则尝试秒传：直接引用已有存储文件并跳过后续传输。
    if (hash_kv && hash_kv[0] != '\0') {
        char reused_storage[256] = {0};
        long long reused_size = 0;
        int hash_rc = db_find_file_by_hash(db, hash_kv, reused_storage, sizeof(reused_storage), &reused_size);
        if (hash_rc == -1) {
            send_kv_response(client_fd, "result=fail&error=hash lookup failed");
            return;
        }
        if (hash_rc == 0) {
            if (db_upsert_file_entry(db, username, parent_path, display_name, reused_storage, hash_kv, reused_size) != 0) {
                send_kv_response(client_fd, "result=fail&error=metadata write failed");
                return;
            }
            remove_storage_file_if_unused(db, base_path, old_storage_name, reused_storage);

            char fast_resp[1024] = {0};
            snprintf(fast_resp,
                     sizeof(fast_resp),
                     "result=fast&path=%s&size=%lld&expected=%lld",
                     logical_path,
                     reused_size,
                     expect_total);
            send_kv_response(client_fd, fast_resp);
            return;
        }
    }

    // 5) 准备真实存储文件（统一放在 base_path 下，文件名随机生成）。
    char storage_name[256] = {0};
    char storage_path[PATH_MAX] = {0};
    int fd = -1;
    for (int attempt = 0; attempt < 5; attempt++) {
        generate_storage_name(storage_name, sizeof(storage_name));
        snprintf(storage_path, sizeof(storage_path), "%s/%s", base_path, storage_name);
        fd = open(storage_path, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, 0664);
        if (fd >= 0) break;
        if (errno != EEXIST) break;
    }
    if (fd < 0) {
        // 多次尝试仍然失败，推测是磁盘或权限问题。
        send_kv_response(client_fd, "result=fail&error=create storage failed");
        return;
    }

    // 6) 告知客户端可以开始传输。
    send_kv_response(client_fd, "result=ok");

    // 7) 循环接收分片数据并写入统一存储目录。
    char buffer[8192];
    ssize_t rlen;
    long long total = 0;
    while (1) {
        rlen = recv_frame(client_fd, buffer, sizeof(buffer));
        if (rlen < 0) {
            close(fd);
            unlink(storage_path);
            // 上传过程中任何异常都需要删除已经写入的临时文件。
            send_kv_response(client_fd, "result=recv failed");
            return;
        }
        if (rlen == 0) break;
        ssize_t written = 0;
        while (written < rlen) {
            ssize_t w = write(fd, buffer + written, (size_t)(rlen - written));
            if (w <= 0) {
                close(fd);
                unlink(storage_path);
                send_kv_response(client_fd, "result=write failed");
                return;
            }
            written += w;
        }
        total += rlen;
    }
    close(fd);

    // 8) 写入或更新数据库元数据。
    if (db_upsert_file_entry(db, username, parent_path, display_name, storage_name, hash_kv, total) != 0) {
        unlink(storage_path);
        // 元数据落库失败时也需要删掉物理文件，避免脏数据。
        send_kv_response(client_fd, "result=fail&error=metadata write failed");
        return;
    }

    // 9) 删除旧文件（若存在且不再被引用）。
    remove_storage_file_if_unused(db, base_path, old_storage_name, storage_name);

    // 10) 回包最终结果。
    char final_resp[1024] = {0};
    snprintf(final_resp,
             sizeof(final_resp),
             "result=ok&path=%s&size=%lld&expected=%lld",
             logical_path,
             total,
             expect_total);
    send_kv_response(client_fd, final_resp);
}
