#include "mkdir.h"
#include "common.h"

#include <sys/stat.h>
#include <string.h>

/*
 * MKDIR：将 arg 解析为目标路径进行创建。
 */
void modules_mkdir_handle(int client_fd, const char* base_path, const char* username, const char* pwd, const char* arg) {
    // 解析目标目录真实路径
    char abs_path[4096] = {0};
    normalize_join_path(base_path, username, pwd, arg, abs_path, sizeof(abs_path), NULL, 0);

    // 创建目录（权限 0775，可按需调整 umask）
    if (mkdir(abs_path, 0775) == 0) {
        send_kv_response(client_fd, "result=ok");
    } else {
        // 失败时返回固定错误文案；如需更细化可结合 errno
        char kv[256] = {0};
        snprintf(kv, sizeof(kv), "result=%s", "mkdir failed");
        send_kv_response(client_fd, kv);
    }
}


