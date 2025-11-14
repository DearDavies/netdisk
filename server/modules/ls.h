#ifndef SERVER_MODULES_LS_H
#define SERVER_MODULES_LS_H

#include "db.h"

/*
 * 处理 LS 命令：从数据库读取当前逻辑目录下的所有条目，并以 \n 拼接返回。
 * 目录名称后会追加 '/' 以便客户端区分。
 */
void modules_ls_handle(int client_fd,
                       const char* base_path,
                       const char* username,
                       const char* pwd,
                       db_handle_t* db);

#endif // SERVER_MODULES_LS_H
