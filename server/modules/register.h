#ifndef SERVER_MODULES_REGISTER_H
#define SERVER_MODULES_REGISTER_H

#include "db.h"

/*
 * 处理注册请求：
 * - 检查用户名是否已存在
 * - 将用户名和 SHA512 密码哈希存储到数据库
 * 返回：通过 send_kv_response 发送 "result=ok" 或 "result=fail&error=..."
 */
void modules_register_handle(int client_fd, const char* username, const char* password_hash, db_handle_t* db);

#endif // SERVER_MODULES_REGISTER_H

