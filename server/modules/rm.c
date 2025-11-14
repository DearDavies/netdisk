#include "rm.h"
#include "common.h"

#include <string.h>
#include <stdio.h>
#include <limits.h>

/*
 * RM：逻辑删除文件或目录。
 * - 文件：直接将 files 表中的 is_deleted 置为 1。
 * - 目录：若非空且 confirm_flag != "yes"，先提示客户端确认；确认后递归地将目录树标记为删除。
 * - 所有操作仅修改数据库标记，不会触碰真实物理文件。
 */
void modules_rm_handle(int client_fd,
                       const char* base_path,
                       const char* username,
                       const char* pwd,
                       const char* arg,
                       const char* confirm_flag,
                       db_handle_t* db) {
    (void)base_path; // 逻辑删除不依赖真实物理路径

    if (!db || !db->conn || !username) {
        send_kv_response(client_fd, "result=fail&error=database not ready");
        return;
    }
    if (!arg || arg[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=missing path");
        return;
    }

    // 规范化出目标逻辑路径，确保始终以 '/' 开头。
    char target_path[PATH_MAX] = {0};
    build_logical_path(pwd, arg, target_path, sizeof(target_path));
    if (target_path[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=invalid path");
        return;
    }
    if (strcmp(target_path, "/") == 0) {
        send_kv_response(client_fd, "result=fail&error=cannot remove root");
        return;
    }

    // 用户输入 y 之后，客户端会携带 confirm=yes，再次进入该流程。
    int force = (confirm_flag && strcasecmp(confirm_flag, "yes") == 0);

    // 先判断是否为目录。
    int dir_state = db_directory_exists(db, username, target_path);
    if (dir_state == -1) {
        send_kv_response(client_fd, "result=fail&error=dir lookup failed");
        return;
    }
    if (dir_state == 0) {
        // 目录存在：若目录非空而用户尚未确认，则提示客户端确认。
        int is_empty = 0;
        if (db_is_directory_empty(db, username, target_path, &is_empty) != 0) {
            send_kv_response(client_fd, "result=fail&error=check dir failed");
            return;
        }
        if (!is_empty && !force) {
            char resp[PATH_MAX + 128];
            snprintf(resp, sizeof(resp),
                     "result=need_confirm&message=directory not empty&path=%s",
                     target_path);
            send_kv_response(client_fd, resp);
            return;
        }
        // 已确认或目录为空，执行逻辑删除（目录及其子树全部置 is_deleted=1）。
        if (db_mark_directory_deleted(db, username, target_path) != 0) {
            send_kv_response(client_fd, "result=fail&error=delete dir failed");
            return;
        }
        send_kv_response(client_fd, "result=ok");
        return;
    }

    // 非目录则尝试按文件处理：拆出父目录与文件名。
    char parent_path[PATH_MAX] = {0};
    char display_name[256] = {0};
    split_parent_and_name(target_path, parent_path, sizeof(parent_path), display_name, sizeof(display_name));
    if (display_name[0] == '\0') {
        send_kv_response(client_fd, "result=fail&error=invalid path");
        return;
    }

    db_file_detail_t detail = {0};
    int file_rc = db_get_entry_by_path(db, username, parent_path, display_name, &detail);
    if (file_rc == -1) {
        send_kv_response(client_fd, "result=fail&error=metadata query failed");
        return;
    }
    if (file_rc == 0) {
        int mark_rc = db_mark_file_deleted(db, username, parent_path, display_name);
        if (mark_rc == -1) {
            send_kv_response(client_fd, "result=fail&error=delete file failed");
            return;
        }
        if (mark_rc == 1) {
            send_kv_response(client_fd, "result=fail&error=not found");
            return;
        }
        send_kv_response(client_fd, "result=ok");
        return;
    }

    // 既不是目录也不是文件
    send_kv_response(client_fd, "result=fail&error=not found");
}
