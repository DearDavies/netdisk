#include "register.h"
#include "db.h"
#include "common.h"
#include "../logger.h"
#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * 处理注册请求：
 * 1) 检查用户名是否已存在
 * 2) 若不存在，将用户名和密码哈希插入数据库
 * 3) 返回结果给客户端
 */
void modules_register_handle(int client_fd, const char* username, const char* password_hash, db_handle_t* db) {
    if (!username || !password_hash || !db || !db->conn) {
        send_kv_response(client_fd, "result=fail&error=invalid request");
        return;
    }
    LOG_DEBUG("进入了server的注册步骤");
    
    // 转义用户名和密码哈希，防止 SQL 注入
    char escaped_username[201] = {0};
    char escaped_password[257] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(db->conn, escaped_password, password_hash, strlen(password_hash));
    
    // 先查询用户名是否已存在
    char query[512];
    snprintf(query, sizeof(query), 
             "SELECT username FROM users WHERE username='%s'", 
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
    
    // 如果已存在该用户名
    if (mysql_num_rows(result) > 0) {
        mysql_free_result(result);
        send_kv_response(client_fd, "result=fail&error=username already exists");
        return;
    }
    mysql_free_result(result);
    
    // 插入新用户
    snprintf(query, sizeof(query),
             "INSERT INTO users (username, password_hash) VALUES ('%s', '%s')",
             escaped_username, escaped_password);
    
    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("插入用户失败：%s", mysql_error(db->conn));
        send_kv_response(client_fd, "result=fail&error=database error");
        return;
    }
    
    LOG_INFO("用户注册成功：%s", username);
    send_kv_response(client_fd, "result=ok");
}

