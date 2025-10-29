#include "db.h"
#include "../read_config.h"
#include "../logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * 初始化数据库连接：从 server_config.ini 的 [database] 节读取连接参数。
 * 配置项：host, port, user, password, database
 */
int db_init(db_handle_t* db) {
    const char* config_file_name = "server_config.ini";
    if (!db) return -1;

    // 从配置文件读取数据库连接参数
    Section* config = parse_ini_file(config_file_name);
    if (!config) {
        LOG_ERROR("无法读取配置文件%s", config_file_name);
        return -1;
    }

    const char* host = get_config_value(config, "database", "host");
    const char* port_str = get_config_value(config, "database", "port");
    const char* user = get_config_value(config, "database", "user");
    const char* password = get_config_value(config, "database", "password");
    const char* database = get_config_value(config, "database", "database");

    // 若未配置，使用默认值
    if (!host) host = "localhost";
    if (!port_str) port_str = "3306";
    if (!user) user = "root";
    if (!password) password = "";
    if (!database) database = "netdisk";

    unsigned int port = (unsigned int)atoi(port_str);

    // 初始化 MySQL 连接
    db->conn = mysql_init(NULL);
    if (!db->conn) {
        LOG_ERROR("mysql_init 失败");
        free_config(config);
        return -1;
    }

    // 连接数据库
    if (!mysql_real_connect(db->conn, host, user, password, database, port, NULL, 0)) {
        LOG_ERROR("mysql_real_connect 失败：%s", mysql_error(db->conn));
        mysql_close(db->conn);
        db->conn = NULL;
        free_config(config);
        return -1;
    }

    // 设置字符集为 UTF-8
    if (mysql_set_character_set(db->conn, "utf8mb4") != 0) {
        LOG_ERROR("设置字符集失败：%s", mysql_error(db->conn));
    }

    LOG_INFO("数据库连接成功：host=%s, database=%s", host, database);
    free_config(config);
    return 0;
}

/*
 * 关闭数据库连接。
 */
void db_close(db_handle_t* db) {
    if (db && db->conn) {
        mysql_close(db->conn);
        db->conn = NULL;
        LOG_INFO("数据库连接已关闭");
    }
}

/*
 * 确保用户表存在：若不存在则创建。
 * 表结构：
 *   - username VARCHAR(100) PRIMARY KEY
 *   - password_hash CHAR(128) NOT NULL
 */
int db_ensure_table(db_handle_t* db) {
    if (!db || !db->conn) return -1;

    const char* create_table_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "uid INT AUTO_INCREMENT PRIMARY KEY,"
        "username VARCHAR(100) NOT NULL,"
        "password_hash CHAR(128) NOT NULL"
        ")";

    if (mysql_query(db->conn, create_table_sql) != 0) {
        LOG_ERROR("创建用户表失败：%s", mysql_error(db->conn));
        return -1;
    }

    LOG_INFO("用户表已确保存在");
    return 0;
}
