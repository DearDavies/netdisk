#ifndef SERVER_MODULES_RM_H
#define SERVER_MODULES_RM_H

#include "db.h"

/*
 * 处理 RM 命令：逻辑删除文件或目录。
 * - 目录若非空且 confirm!=yes，会先回传 result=need_confirm 等待客户端确认；
 * - 仅做逻辑删除（is_deleted=1），不会移除真实文件。
 */
void modules_rm_handle(int client_fd,
                       const char* base_path,
                       const char* username,
                       const char* pwd,
                       const char* arg,
                       const char* confirm_flag,
                       db_handle_t* db);

#endif // SERVER_MODULES_RM_H
