#ifndef SERVER_MODULES_CD_H
#define SERVER_MODULES_CD_H

/*
 * 处理 CD 命令：根据 arg 计算新目录并验证存在性。
 * 成功：返回 "pwd=<新路径>&result=ok"；失败："pwd=<原路径>&result=<错误>"。
 */
#include "db.h"

void modules_cd_handle(int client_fd,
                       const char* base_path,
                       const char* username,
                       const char* pwd,
                       const char* arg,
                       db_handle_t* db);

#endif // SERVER_MODULES_CD_H
