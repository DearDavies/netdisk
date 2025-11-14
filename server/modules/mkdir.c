#include "mkdir.h"
#include "common.h"

#include <string.h>
#include <stdio.h>
#include <limits.h>

/*
 * MKDIR：依赖数据库记录维护逻辑目录结构，而非真实物理路径。
 * 流程：
 * 1. 解析目标逻辑路径（基于当前 pwd + arg）并校验合法性；
 * 2. 确认父目录存在且为目录；
 * 3. 确认同名条目不存在；
 * 4. 在 directories 表中新增目录记录（仅逻辑层面变化）。
 */
void modules_mkdir_handle(int client_fd,
                          const char* base_path,
                          const char* username,
                          const char* pwd,
                          const char* arg,
                          db_handle_t* db) {
    (void)base_path; // 逻辑目录不再依赖真实物理路径

    if (!db || !db->conn || !username) {
        send_kv_response(client_fd, "result=fail&error=database not ready");
        return;
    }
    if (!arg || arg[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=missing directory name");
        return;
    }

    // 1) 规范化目标逻辑路径，确保始终以 '/' 开头且处理 '.', '..'。
    char target_path[PATH_MAX] = {0};
    build_logical_path(pwd, arg, target_path, sizeof(target_path));
    if (strcmp(target_path, "/") == 0) {
        send_kv_response(client_fd, "result=fail&error=invalid path");
        return;
    }

    // 拆分父目录与新目录名称，便于后续数据库查询。
    char parent_path[PATH_MAX] = {0};
    char dir_name[256] = {0};
    split_parent_and_name(target_path, parent_path, sizeof(parent_path), dir_name, sizeof(dir_name));
    if (dir_name[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=invalid path");
        return;
    }

    // 2) 确认父目录存在，并先检查目标目录是否已存在。
    int target_state = db_directory_exists(db, username, target_path);
    if (target_state == -1) {
        send_kv_response(client_fd, "result=fail&error=dir lookup failed");
        return;
    }
    if (target_state == 0) {
        send_kv_response(client_fd, "result=fail&error=already exists");
        return;
    }

    int parent_state = db_directory_exists(db, username, parent_path);
    if (parent_state == -1) {
        send_kv_response(client_fd, "result=fail&error=dir lookup failed");
        return;
    }
    if (parent_state == 1) {
        send_kv_response(client_fd, "result=fail&error=parent not exists");
        return;
    }

    // 3) 检查是否存在同名文件，避免目录名与文件冲突。
    db_file_detail_t detail = {0};
    int file_rc = db_get_entry_by_path(db, username, parent_path, dir_name, &detail);
    if (file_rc == -1) {
        send_kv_response(client_fd, "result=fail&error=metadata query failed");
        return;
    }
    if (file_rc == 0) {
        send_kv_response(client_fd, "result=fail&error=name conflict with file");
        return;
    }

    // 4) 插入目录记录：写入 directories 表以维护逻辑路径树。
    if (db_insert_directory(db, username, parent_path, dir_name) != 0) {
        send_kv_response(client_fd, "result=fail&error=insert dir failed");
        return;
    }

    char kv[512] = {0};
    snprintf(kv, sizeof(kv), "result=ok&path=%s", target_path);
    send_kv_response(client_fd, kv);
}
