#include "cd.h"
#include "common.h"

#include <sys/stat.h>
#include <string.h>

/*
 * CD：规范化出新 pwd，并验证目标是否存在且为目录。
 */
void modules_cd_handle(int client_fd, const char* base_path, const char* username, const char* pwd, const char* arg) {
    // 计算规范化后的逻辑路径 new_pwd 与对应真实路径 abs_path
    char abs_path[4096] = {0};
    char new_pwd[4096] = {0};
    normalize_join_path(base_path, username, pwd, arg, abs_path, sizeof(abs_path), new_pwd, sizeof(new_pwd));

    // 校验目标是否存在且为目录
    struct stat st;
    if (stat(abs_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        // 成功：回传新的逻辑 pwd 与 ok
        char kv[1024] = {0};
        snprintf(kv, sizeof(kv), "pwd=%s&result=ok", new_pwd);
        send_kv_response(client_fd, kv);
    } else {
        // 失败：维持原 pwd，并说明原因
        char kv[256] = {0};
        snprintf(kv, sizeof(kv), "pwd=%s&result=%s", pwd ? pwd : "/", "no such directory");
        send_kv_response(client_fd, kv);
    }
}


