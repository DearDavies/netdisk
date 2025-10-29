#include "login.h"
#include "db.h"
#include "common.h"
#include "../logger.h"
#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * 处理登录请求：
 * 1) 从数据库查询用户名对应的密码哈希
 * 2) 比对客户端传来的密码哈希
 * 3) 返回结果给客户端
 */
void modules_login_handle(int client_fd, const char* username, const char* password_hash, db_handle_t* db) {
    if (!username || !password_hash || !db || !db->conn) {
        send_kv_response(client_fd, "result=fail&error=invalid request");
        return;
    }
    
    // 转义用户名，防止 SQL 注入
    char escaped_username[201] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    
    // 查询用户信息
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT password_hash FROM users WHERE username='%s'",
             escaped_username);
    
    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("查询用户失败：%s", mysql_error(db->conn));
        send_kv_response(client_fd, "result=fail&error=database error");
        return;
    }
    
    MYSQL_RES* result = mysql_store_result(db->conn);
    if (!result) {
        LOG_ERROR("获取查询结果失败：%s", mysql_error(db->conn));
        send_kv_response(client_fd, "result=fail&error=database error");
        return;
    }
    
    // 检查用户是否存在
    if (mysql_num_rows(result) == 0) {
        mysql_free_result(result);
        send_kv_response(client_fd, "result=fail&error=username not found");
        return;
    }
    
    // 获取密码哈希
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row || !row[0]) {
        mysql_free_result(result);
        send_kv_response(client_fd, "result=fail&error=database error");
        return;
    }
    
    const char* stored_hash = row[0];
    
    // 比对密码哈希（SHA512 输出为 128 字符的十六进制字符串）
    if (strlen(stored_hash) != 128 || strcmp(stored_hash, password_hash) != 0) {
        mysql_free_result(result);
        send_kv_response(client_fd, "result=fail&error=password incorrect");
        return;
    }
    
    mysql_free_result(result);
    LOG_INFO("用户登录成功：%s", username);
    send_kv_response(client_fd, "result=ok");
}

