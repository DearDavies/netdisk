#include "cd.h"
#include "common.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

/*
 * CD：仅操作逻辑目录，依据数据库记录判断目标路径是否存在。
 * 1. 组合出目标逻辑路径；
 * 2. 若为根目录则直接允许；
 * 3. 否则查询 files 表确认目录存在；
 * 4. 成功返回新的 pwd，失败返回原 pwd 与错误原因。
 */
void modules_cd_handle(int client_fd,
                       const char* base_path,
                       const char* username,
                       const char* pwd,
                       const char* arg,
                       db_handle_t* db) {
    (void)base_path; // 逻辑目录完全靠数据库，保留参数以兼容旧接口

    if (!db || !db->conn || !username) {
        send_kv_response(client_fd, "pwd=/&result=fail&error=database not ready");
        return;
    }

    // 计算目标逻辑路径
    char target_pwd[PATH_MAX] = {0};
    build_logical_path(pwd, arg, target_pwd, sizeof(target_pwd));

    int dir_state = db_directory_exists(db, username, target_pwd);
    if (dir_state == -1) {
        char kv[256] = {0};
        snprintf(kv, sizeof(kv), "pwd=%s&result=fail&error=dir lookup failed", pwd ? pwd : "/");
        send_kv_response(client_fd, kv);
        return;
    }
    if (dir_state == 1) {
        char kv[256] = {0};
        snprintf(kv, sizeof(kv), "pwd=%s&result=no such directory", pwd ? pwd : "/");
        send_kv_response(client_fd, kv);
        return;
    }

    char kv[512] = {0};
    snprintf(kv, sizeof(kv), "pwd=%s&result=ok", target_pwd);
    send_kv_response(client_fd, kv);
}
