#ifndef SERVER_MODULES_LOGIN_H
#define SERVER_MODULES_LOGIN_H

#include "db.h"

/*
 * 处理登录请求：
 * - 从数据库查询用户名和密码哈希
 * - 比对客户端传来的密码哈希
 * 返回：通过 send_kv_response 发送 "result=ok" 或 "result=fail&error=..."
 */
void modules_login_handle(int client_fd, const char* username, const char* password_hash, db_handle_t* db);

#endif // SERVER_MODULES_LOGIN_H

