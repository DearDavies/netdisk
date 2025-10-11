#include "rm.h"
#include "common.h"

#include <unistd.h>
#include <string.h>

/*
 * RM：若为目录，仅支持删除空目录；否则按文件删除。
 */
void modules_rm_handle(int client_fd, const char* base_path, const char* username, const char* pwd, const char* arg) {
    // 解析目标真实路径
    char abs_path[4096] = {0};
    normalize_join_path(base_path, username, pwd, arg, abs_path, sizeof(abs_path), NULL, 0);

    // 目录：仅删除空目录；否则按文件删除
    int ok = 0;
    if (is_dir(abs_path)) {
        if (rmdir(abs_path) == 0) ok = 1; // 仅支持删除空目录
    } else {
        if (unlink(abs_path) == 0) ok = 1;
    }

    // 返回结果
    if (ok) send_kv_response(client_fd, "result=ok");
    else send_kv_response(client_fd, "result=rm failed");
}


