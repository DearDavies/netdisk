#include "db.h"
#include "common.h"
#include "../read_config.h"
#include "../logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/*
 * 为 LIKE 模式构建“dir_path(去重复 '/') + '/%'”形式的字符串，并对 '%'' '_' '\' 做转义，
 * 方便在 SQL 中匹配某目录下的所有子项（包括多级子目录）。
 */
static void build_child_like_pattern(const char* dir_path, char* out, size_t out_sz) {
    size_t j = 0;
    size_t len = dir_path ? strlen(dir_path) : 0;
    for (size_t i = 0; i < len && j + 1 < out_sz; ++i) {
        char ch = dir_path[i];
        if (ch == '%' || ch == '_' || ch == '\\') {
            if (j + 2 >= out_sz) break;
            out[j++] = '\\';
        }
        out[j++] = ch;
    }
    if (len == 0 || dir_path[len - 1] != '/') {
        if (j + 1 < out_sz) {
            out[j++] = '/';
        }
    }
    if (j + 1 < out_sz) {
        out[j++] = '%';
    }
    out[j] = '\0';
}
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

    const char* create_users_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "uid INT AUTO_INCREMENT PRIMARY KEY,"
        "username VARCHAR(100) NOT NULL UNIQUE,"
        "password_hash CHAR(128) NOT NULL"
        ")";

    if (mysql_query(db->conn, create_users_sql) != 0) {
        LOG_ERROR("创建用户表失败：%s", mysql_error(db->conn));
        return -1;
    }

    const char* create_files_sql =
        "CREATE TABLE IF NOT EXISTS files ("
        "fid BIGINT AUTO_INCREMENT PRIMARY KEY,"
        "owner_username VARCHAR(100) NOT NULL,"
        "display_name VARCHAR(255) NOT NULL,"
        "storage_name VARCHAR(255) DEFAULT NULL,"
        "parent_path VARCHAR(1024) NOT NULL,"
        "parent_path_md5 CHAR(32) NOT NULL,"
        "file_hash CHAR(128) NOT NULL DEFAULT '',"
        "size BIGINT DEFAULT 0,"
        "is_deleted TINYINT(1) NOT NULL DEFAULT 0,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "UNIQUE KEY uniq_owner_path_name (owner_username, parent_path_md5, display_name),"
        "KEY idx_file_hash (file_hash)"
        ")";

    if (mysql_query(db->conn, create_files_sql) != 0) {
        LOG_ERROR("创建文件元数据表失败：%s", mysql_error(db->conn));
        return -1;
    }

    /*
     * directories 表专用于维护逻辑目录树：
     * - full_path 可能超过 InnoDB 的索引长度限制，因此新增 full_path_md5 做唯一约束。
     * - parent_path_md5 便于快速定位父目录下的所有子目录。
     */
    const char* create_dirs_sql =
        "CREATE TABLE IF NOT EXISTS directories ("
        "did BIGINT AUTO_INCREMENT PRIMARY KEY,"
        "owner_username VARCHAR(100) NOT NULL,"
        "display_name VARCHAR(255) NOT NULL,"
        "parent_path VARCHAR(1024) NOT NULL,"
        "parent_path_md5 CHAR(32) NOT NULL,"
        "full_path VARCHAR(1024) NOT NULL,"
        "full_path_md5 CHAR(32) NOT NULL,"
        "is_deleted TINYINT(1) NOT NULL DEFAULT 0,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "UNIQUE KEY uniq_owner_full_path_hash (owner_username, full_path_md5),"
        "KEY idx_owner_parent (owner_username, parent_path_md5)"
        ")";
    if (mysql_query(db->conn, create_dirs_sql) != 0) {
        LOG_ERROR("创建目录表失败：%s", mysql_error(db->conn));
        return -1;
    }

    LOG_INFO("用户表、文件表与目录表已确保存在");
    return 0;
}

/*
 * 查询指定用户在 parent_path 目录下的所有条目，并按目录优先排序。
 * 这里直接返回一个包含 db_file_entry_t 的数组，便于上层遍历后发送给客户端。
 */
int db_list_entries_by_path(db_handle_t* db,
                            const char* username,
                            const char* parent_path,
                            db_file_entry_t** entries,
                            size_t* entry_count) {
    if (!db || !db->conn || !username || !parent_path || !entries || !entry_count) {
        LOG_ERROR("列目录传入的参数不完整");
        return -1;
    }

    // 预留足够空间存放转义后的字符串（MySQL 建议 2n+1）
    char escaped_username[256] = {0};
    char escaped_parent[2049] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(db->conn, escaped_parent, parent_path, strlen(parent_path));

    db_file_entry_t* list = NULL;
    size_t used = 0;
    size_t capacity = 0;

    // 先查询目录表，列出当前 parent_path 下的所有子目录。
    char dir_query[3072] = {0};
    snprintf(dir_query, sizeof(dir_query),
             "SELECT display_name FROM directories "
             "WHERE owner_username='%s' AND parent_path_md5=MD5('%s') AND parent_path='%s' AND is_deleted=0 "
             "ORDER BY display_name ASC",
             escaped_username, escaped_parent, escaped_parent);
    if (mysql_query(db->conn, dir_query) != 0) {
        LOG_ERROR("查询目录表失败：%s", mysql_error(db->conn));
        return -1;
    }
    MYSQL_RES* dir_result = mysql_store_result(db->conn);
    if (!dir_result) {
        LOG_ERROR("获取目录表结果失败：%s", mysql_error(db->conn));
        return -1;
    }
    unsigned long dir_rows = mysql_num_rows(dir_result);
    if (dir_rows > 0) {
        capacity = dir_rows;
        list = (db_file_entry_t*)calloc(capacity, sizeof(db_file_entry_t));
        if (!list) {
            mysql_free_result(dir_result);
            LOG_ERROR("为目录列表分配内存失败");
            return -1;
        }
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(dir_result)) != NULL) {
            const char* name = row[0] ? row[0] : "";
            strncpy(list[used].name, name, sizeof(list[used].name) - 1);
            list[used].is_dir = 1;
            list[used].size = 0;
            used++;
        }
    }
    mysql_free_result(dir_result);

    // 再查询文件表，列出当前目录下的文件。
    char file_query[3072] = {0};
    snprintf(file_query, sizeof(file_query),
             "SELECT display_name,size FROM files "
             "WHERE owner_username='%s' AND parent_path_md5=MD5('%s') AND parent_path='%s' AND is_deleted=0 "
             "ORDER BY display_name ASC",
             escaped_username, escaped_parent, escaped_parent);
    if (mysql_query(db->conn, file_query) != 0) {
        LOG_ERROR("查询文件列表失败：%s", mysql_error(db->conn));
        free(list);
        return -1;
    }
    MYSQL_RES* file_result = mysql_store_result(db->conn);
    if (!file_result) {
        LOG_ERROR("获取文件列表失败：%s", mysql_error(db->conn));
        free(list);
        return -1;
    }
    unsigned long file_rows = mysql_num_rows(file_result);
    if (file_rows > 0) {
        size_t new_cap = used + file_rows;
        db_file_entry_t* tmp = (db_file_entry_t*)realloc(list, new_cap * sizeof(db_file_entry_t));
        if (!tmp) {
            mysql_free_result(file_result);
            free(list);
            LOG_ERROR("扩容文件列表失败");
            return -1;
        }
        list = tmp;
        capacity = new_cap;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(file_result)) != NULL) {
            const char* name = row[0] ? row[0] : "";
            const char* size_str = row[1] ? row[1] : "0";
            strncpy(list[used].name, name, sizeof(list[used].name) - 1);
            list[used].is_dir = 0;
            list[used].size = atoll(size_str);
            used++;
        }
    }
    mysql_free_result(file_result);

    *entries = list;
    *entry_count = used;
    return 0;
}

/*
 * 释放由 db_list_entries_by_path 分配的结果数组。
 */
void db_free_entries(db_file_entry_t* entries) {
    free(entries);
}

/*
 * 将一条文件的元数据写入数据库，如存在则更新。
 */
int db_upsert_file_entry(db_handle_t* db,
                         const char* username,
                         const char* parent_path,
                         const char* display_name,
                         const char* storage_name,
                         const char* file_hash,
                         long long size) {
    if (!db || !db->conn || !username || !parent_path || !display_name) {
        LOG_ERROR("写入文件元数据时参数不完整");
        return -1;
    }

    char escaped_username[256] = {0};
    char escaped_parent[2049] = {0};
    char escaped_display[513] = {0};
    char escaped_storage[4097] = {0};
    char escaped_hash[257] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(db->conn, escaped_parent, parent_path, strlen(parent_path));
    mysql_real_escape_string(db->conn, escaped_display, display_name, strlen(display_name));

    mysql_real_escape_string(db->conn, escaped_hash, file_hash ? file_hash : "", file_hash ? strlen(file_hash) : 0);

    int use_null_storage = (!storage_name || storage_name[0] == '\0');
    if (!use_null_storage) {
        mysql_real_escape_string(db->conn, escaped_storage, storage_name, strlen(storage_name));
    }

    char storage_value[4098] = {0};
    if (use_null_storage) {
        snprintf(storage_value, sizeof(storage_value), "NULL");
    } else {
        snprintf(storage_value, sizeof(storage_value), "'%s'", escaped_storage);
    }

    char query[8192] = {0};
    snprintf(query, sizeof(query),
             "INSERT INTO files (owner_username, display_name, storage_name, parent_path, parent_path_md5, file_hash, size, is_deleted) "
             "VALUES ('%s', '%s', %s, '%s', MD5('%s'), '%s', %lld, 0) "
             "ON DUPLICATE KEY UPDATE "
             "parent_path=VALUES(parent_path), "
             "parent_path_md5=VALUES(parent_path_md5), "
             "file_hash=VALUES(file_hash), "
             "storage_name=VALUES(storage_name), "
             "size=VALUES(size), "
             "is_deleted=0, "
             "updated_at=CURRENT_TIMESTAMP",
             escaped_username,
             escaped_display,
             storage_value,
             escaped_parent,
             escaped_parent,
             escaped_hash,
             size);

    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("写入文件元数据失败：%s", mysql_error(db->conn));
        return -1;
    }
    return 0;
}

int db_insert_directory(db_handle_t* db,
                        const char* username,
                        const char* parent_path,
                        const char* display_name) {
    if (!db || !db->conn || !username || !parent_path || !display_name || display_name[0] == '\0') {
        LOG_ERROR("插入目录时参数不完整");
        return -1;
    }
    if (strcmp(display_name, ".") == 0 || strcmp(display_name, "..") == 0 || strchr(display_name, '/')) {
        LOG_ERROR("非法目录名：%s", display_name);
        return -1;
    }

    char full_path[PATH_MAX] = {0};
    if (strcmp(parent_path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "/%s", display_name);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", parent_path, display_name);
    }

    char escaped_username[256] = {0};
    char escaped_parent[2049] = {0};
    char escaped_display[513] = {0};
    char escaped_full[2049] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(db->conn, escaped_parent, parent_path, strlen(parent_path));
    mysql_real_escape_string(db->conn, escaped_display, display_name, strlen(display_name));
    mysql_real_escape_string(db->conn, escaped_full, full_path, strlen(full_path));

    /*
     * full_path 可能超过索引长度，因此这里使用 MySQL 自带的 MD5 计算哈希，
     * 以 full_path_md5 作为唯一键，既能确保唯一性又能避免索引长度限制。
     * 若目录此前被逻辑删除（is_deleted=1），ON DUPLICATE KEY UPDATE 会将其恢复。
     */
    char query[4096] = {0};
    snprintf(query, sizeof(query),
             "INSERT INTO directories (owner_username, display_name, parent_path, parent_path_md5, full_path, full_path_md5) "
             "VALUES ('%s', '%s', '%s', MD5('%s'), '%s', MD5('%s')) "
             "ON DUPLICATE KEY UPDATE "
             "parent_path=VALUES(parent_path), "
             "parent_path_md5=VALUES(parent_path_md5), "
             "display_name=VALUES(display_name), "
             "full_path=VALUES(full_path), "
             "full_path_md5=VALUES(full_path_md5), "
             "is_deleted=0",
             escaped_username,
             escaped_display,
             escaped_parent,
             escaped_parent,
             escaped_full,
             escaped_full);

    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("写入目录失败：%s", mysql_error(db->conn));
        return -1;
    }
    return 0;
}

/*
 * 按逻辑路径精确查询单条记录。
 */
int db_get_entry_by_path(db_handle_t* db,
                         const char* username,
                         const char* parent_path,
                         const char* display_name,
                         db_file_detail_t* detail) {
    if (!db || !db->conn || !username || !parent_path || !display_name) {
        LOG_ERROR("查询文件元数据时参数不完整");
        return -1;
    }

    char escaped_username[256] = {0};
    char escaped_parent[2049] = {0};
    char escaped_display[513] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(db->conn, escaped_parent, parent_path, strlen(parent_path));
    mysql_real_escape_string(db->conn, escaped_display, display_name, strlen(display_name));

    char query[4096] = {0};
    snprintf(query, sizeof(query),
             "SELECT size,COALESCE(storage_name,''),COALESCE(file_hash,'') FROM files "
             "WHERE owner_username='%s' AND parent_path_md5=MD5('%s') AND parent_path='%s' "
             "AND display_name='%s' AND is_deleted=0 LIMIT 1",
             escaped_username,
             escaped_parent,
             escaped_parent,
             escaped_display);

    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("查询文件元数据失败：%s", mysql_error(db->conn));
        return -1;
    }

    MYSQL_RES* result = mysql_store_result(db->conn);
    if (!result) {
        LOG_ERROR("获取查询结果失败：%s", mysql_error(db->conn));
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return 1; // 不存在
    }

    if (detail) {
        detail->size = row[0] ? atoll(row[0]) : 0;
        detail->storage_name[0] = '\0';
        detail->file_hash[0] = '\0';
        if (row[1]) {
            strncpy(detail->storage_name, row[1], sizeof(detail->storage_name) - 1);
            detail->storage_name[sizeof(detail->storage_name) - 1] = '\0';
        }
        if (row[2]) {
            strncpy(detail->file_hash, row[2], sizeof(detail->file_hash) - 1);
            detail->file_hash[sizeof(detail->file_hash) - 1] = '\0';
        }
    }
    mysql_free_result(result);
    return 0;
}

int db_find_file_by_hash(db_handle_t* db,
                         const char* file_hash,
                         char* storage_name_out,
                         size_t storage_name_sz,
                         long long* size_out) {
    if (!db || !db->conn || !file_hash || file_hash[0] == '\0') {
        return -1;
    }

    char escaped_hash[257] = {0};
    mysql_real_escape_string(db->conn, escaped_hash, file_hash, strlen(file_hash));

    char query[1024] = {0};
    snprintf(query, sizeof(query),
             "SELECT storage_name,size FROM files "
             "WHERE file_hash='%s' AND is_deleted=0 AND storage_name IS NOT NULL AND storage_name<>'' LIMIT 1",
             escaped_hash);

    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("通过哈希查询文件失败：%s", mysql_error(db->conn));
        return -1;
    }

    MYSQL_RES* result = mysql_store_result(db->conn);
    if (!result) {
        LOG_ERROR("获取哈希查询结果失败：%s", mysql_error(db->conn));
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return 1;
    }

    if (storage_name_out && storage_name_sz > 0 && row[0]) {
        strncpy(storage_name_out, row[0], storage_name_sz - 1);
        storage_name_out[storage_name_sz - 1] = '\0';
    }
    if (size_out) {
        *size_out = row[1] ? atoll(row[1]) : 0;
    }
    mysql_free_result(result);
    return 0;
}

int db_storage_refcount(db_handle_t* db,
                        const char* storage_name,
                        int* ref_count) {
    if (!db || !db->conn || !storage_name || storage_name[0] == '\0') {
        return -1;
    }

    char escaped_name[512] = {0};
    mysql_real_escape_string(db->conn, escaped_name, storage_name, strlen(storage_name));

    char query[1024] = {0};
    snprintf(query, sizeof(query),
             "SELECT COUNT(*) FROM files WHERE storage_name='%s' AND is_deleted=0",
             escaped_name);

    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("统计存储引用失败：%s", mysql_error(db->conn));
        return -1;
    }

    MYSQL_RES* result = mysql_store_result(db->conn);
    if (!result) {
        LOG_ERROR("获取存储统计结果失败：%s", mysql_error(db->conn));
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return -1;
    }

    if (ref_count) {
        *ref_count = row[0] ? atoi(row[0]) : 0;
    }
    mysql_free_result(result);
    return 0;
}

int db_directory_exists(db_handle_t* db,
                        const char* username,
                        const char* dir_path) {
    if (!db || !db->conn || !username || !dir_path || dir_path[0] == '\0') {
        return -1;
    }
    if (strcmp(dir_path, "/") == 0) {
        return 0; // 根目录永远存在
    }

    char escaped_username[256] = {0};
    char escaped_full[2049] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(db->conn, escaped_full, dir_path, strlen(dir_path));

    /*
     * 通过 full_path_md5 命中索引，同时再次比对 full_path 本身，避免极低概率的哈希碰撞。
     */
    char query[1024] = {0};
    snprintf(query, sizeof(query),
             "SELECT 1 FROM directories "
             "WHERE owner_username='%s' AND full_path_md5=MD5('%s') AND full_path='%s' AND is_deleted=0 LIMIT 1",
             escaped_username,
             escaped_full,
             escaped_full);

    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("查询目录存在性失败：%s", mysql_error(db->conn));
        return -1;
    }
    MYSQL_RES* result = mysql_store_result(db->conn);
    if (!result) {
        LOG_ERROR("获取目录存在性查询结果失败：%s", mysql_error(db->conn));
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    int status = row ? 0 : 1;
    mysql_free_result(result);
    return status;
}

int db_mark_file_deleted(db_handle_t* db,
                         const char* username,
                         const char* parent_path,
                         const char* display_name) {
    // 将 files 表中的单个文件标记为 is_deleted=1。
    if (!db || !db->conn || !username || !parent_path || !display_name) {
        return -1;
    }
    char escaped_username[256] = {0};
    char escaped_parent[2049] = {0};
    char escaped_display[513] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(db->conn, escaped_parent, parent_path, strlen(parent_path));
    mysql_real_escape_string(db->conn, escaped_display, display_name, strlen(display_name));

    char query[2048] = {0};
    snprintf(query, sizeof(query),
             "UPDATE files SET is_deleted=1 "
             "WHERE owner_username='%s' AND parent_path_md5=MD5('%s') AND parent_path='%s' "
             "AND display_name='%s' AND is_deleted=0",
             escaped_username,
             escaped_parent,
             escaped_parent,
             escaped_display);

    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("逻辑删除文件失败：%s", mysql_error(db->conn));
        return -1;
    }
    my_ulonglong affected = mysql_affected_rows(db->conn);
    return affected > 0 ? 0 : 1;
}

int db_is_directory_empty(db_handle_t* db,
                          const char* username,
                          const char* dir_path,
                          int* is_empty) {
    // 统计某目录下是否还存在未删除的子目录或文件，结果通过 is_empty 返回。
    if (!db || !db->conn || !username || !dir_path || !is_empty) {
        return -1;
    }
    char escaped_username[256] = {0};
    char escaped_dir[2049] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(db->conn, escaped_dir, dir_path, strlen(dir_path));

    char query[2048] = {0};
    snprintf(query, sizeof(query),
             "SELECT COUNT(*) FROM directories "
             "WHERE owner_username='%s' AND parent_path_md5=MD5('%s') AND parent_path='%s' AND is_deleted=0",
             escaped_username,
             escaped_dir,
             escaped_dir);
    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("统计子目录数量失败：%s", mysql_error(db->conn));
        return -1;
    }
    MYSQL_RES* result = mysql_store_result(db->conn);
    if (!result) {
        LOG_ERROR("获取子目录数量失败：%s", mysql_error(db->conn));
        return -1;
    }
    MYSQL_ROW row = mysql_fetch_row(result);
    long long dir_count = row && row[0] ? atoll(row[0]) : 0;
    mysql_free_result(result);

    snprintf(query, sizeof(query),
             "SELECT COUNT(*) FROM files "
             "WHERE owner_username='%s' AND parent_path_md5=MD5('%s') AND parent_path='%s' AND is_deleted=0",
             escaped_username,
             escaped_dir,
             escaped_dir);
    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("统计子文件数量失败：%s", mysql_error(db->conn));
        return -1;
    }
    result = mysql_store_result(db->conn);
    if (!result) {
        LOG_ERROR("获取子文件数量失败：%s", mysql_error(db->conn));
        return -1;
    }
    row = mysql_fetch_row(result);
    long long file_count = row && row[0] ? atoll(row[0]) : 0;
    mysql_free_result(result);

    *is_empty = (dir_count == 0 && file_count == 0) ? 1 : 0;
    return 0;
}

int db_mark_directory_deleted(db_handle_t* db,
                              const char* username,
                              const char* dir_path) {
    // 将目录及其所有子目录、子文件批量标记为 is_deleted=1。
    if (!db || !db->conn || !username || !dir_path || dir_path[0] == '\0') {
        return -1;
    }
    char escaped_username[256] = {0};
    char escaped_full[2049] = {0};
    mysql_real_escape_string(db->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(db->conn, escaped_full, dir_path, strlen(dir_path));

    char like_raw[4096] = {0};
    build_child_like_pattern(dir_path, like_raw, sizeof(like_raw));
    char escaped_like[4096] = {0};
    mysql_real_escape_string(db->conn, escaped_like, like_raw, strlen(like_raw));

    char query[8192] = {0};
    snprintf(query, sizeof(query),
             "UPDATE directories SET is_deleted=1 "
             "WHERE owner_username='%s' AND is_deleted=0 AND "
             "((full_path_md5=MD5('%s') AND full_path='%s') "
             "OR (full_path LIKE '%s' ESCAPE '\\\\'))",
             escaped_username,
             escaped_full,
             escaped_full,
             escaped_like);
    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("逻辑删除目录失败：%s", mysql_error(db->conn));
        return -1;
    }

    char file_like_raw[4096] = {0};
    build_child_like_pattern(dir_path, file_like_raw, sizeof(file_like_raw));
    char file_like_escaped[4096] = {0};
    mysql_real_escape_string(db->conn, file_like_escaped, file_like_raw, strlen(file_like_raw));

    snprintf(query, sizeof(query),
             "UPDATE files SET is_deleted=1 "
             "WHERE owner_username='%s' AND is_deleted=0 AND "
             "((parent_path_md5=MD5('%s') AND parent_path='%s') "
             "OR (parent_path LIKE '%s' ESCAPE '\\\\'))",
             escaped_username,
             escaped_full,
             escaped_full,
             file_like_escaped);
    if (mysql_query(db->conn, query) != 0) {
        LOG_ERROR("逻辑删除目录下文件失败：%s", mysql_error(db->conn));
        return -1;
    }
    return 0;
}
