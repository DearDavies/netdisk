#ifndef SERVER_MODULES_DB_H
#define SERVER_MODULES_DB_H

#include <mysql/mysql.h>

/*
 * 数据库连接结构体
 */
typedef struct {
    MYSQL* conn;  // MySQL 连接句柄
} db_handle_t;

/*
 * 初始化数据库连接（从配置文件读取连接参数）。
 * 返回：0 成功，-1 失败
 */
int db_init(db_handle_t* db);

/*
 * 关闭数据库连接。
 */
void db_close(db_handle_t* db);

/*
 * 确保用户表存在，若不存在则创建。
 * 表结构：username VARCHAR(100) PRIMARY KEY, password_hash CHAR(128)
 * 返回：0 成功，-1 失败
 */
int db_ensure_table(db_handle_t* db);

#endif // SERVER_MODULES_DB_H

