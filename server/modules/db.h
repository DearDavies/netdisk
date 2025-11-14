#ifndef SERVER_MODULES_DB_H
#define SERVER_MODULES_DB_H

#include <mysql/mysql.h>
#include <stddef.h>

/*
 * 数据库连接结构体
 */
typedef struct {
    MYSQL* conn;  // MySQL 连接句柄
} db_handle_t;

/*
 * 以结构体形式描述 files 表中一条记录的核心信息，供上层模块使用。
 * name      ：逻辑目录下展现给用户的名称
 * is_dir    ：标记是否目录（1 为目录，0 为文件）
 * size      ：文件大小（目录恒为 0）
 */
typedef struct {
    char name[256];
    int is_dir;
    long long size;
} db_file_entry_t;

/*
 * 描述 files 表中一条记录的完整信息，用于按路径精确查询。
 */
typedef struct {
    long long size;
    char storage_name[256];
    char file_hash[129];
} db_file_detail_t;

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
 * 确保所有需要的表存在，若不存在则创建（包括 users / files 等）。
 * 返回：0 成功，-1 失败
 */
int db_ensure_table(db_handle_t* db);

/*
 * 列出某个用户在指定逻辑目录(parent_path)下的条目。
 * - username    ：用户名
 * - parent_path ：逻辑目录（以 '/' 开头，例如 "/docs"）
 * - entries     ：输出数组指针，由函数内部分配，调用者负责 db_free_entries
 * - entry_count ：输出条目数量
 * 返回：0 成功，-1 失败
 */
int db_list_entries_by_path(db_handle_t* db,
                            const char* username,
                            const char* parent_path,
                            db_file_entry_t** entries,
                            size_t* entry_count);

/*
 * 释放 db_list_entries_by_path 返回的动态数组。
 */
void db_free_entries(db_file_entry_t* entries);

/*
 * 向 files 表写入或更新单条文件元数据（不再承担目录信息）。
 * - username      ：所属用户
 * - parent_path   ：逻辑父目录（以 '/' 开头）
 * - display_name  ：在该目录下展示的名称
 * - storage_name  ：实际存储位置（可填绝对路径或对象键）
 * - size          ：文件大小
 * 成功返回 0，失败返回 -1。
 */
int db_upsert_file_entry(db_handle_t* db,
                         const char* username,
                         const char* parent_path,
                         const char* display_name,
                         const char* storage_name,
                         const char* file_hash,
                         long long size);

/*
 * 向目录表写入一条目录记录。
 * 成功返回 0；若已存在或数据库错误则返回 -1。
 */
int db_insert_directory(db_handle_t* db,
                        const char* username,
                        const char* parent_path,
                        const char* display_name);

/*
 * 根据 (username, parent_path, display_name) 精确获取单条记录。
 * 返回值：0 查询到；1 表示不存在；-1 数据库错误。
 */
int db_get_entry_by_path(db_handle_t* db,
                         const char* username,
                         const char* parent_path,
                         const char* display_name,
                         db_file_detail_t* detail);

/*
 * 通过文件哈希查找任意一条已存在的记录，返回其存储文件名与大小。
 * 返回：0 找到；1 未找到；-1 数据库错误。
 */
int db_find_file_by_hash(db_handle_t* db,
                         const char* file_hash,
                         char* storage_name_out,
                         size_t storage_name_sz,
                         long long* size_out);

/*
 * 查询某个 storage_name 仍被多少条记录引用，返回 0 表示查询成功。
 */
int db_storage_refcount(db_handle_t* db,
                        const char* storage_name,
                        int* ref_count);

/*
 * 检查给定逻辑路径是否存在且为目录。
 * 返回：0 存在且为目录；1 不存在或不是目录；-1 其他数据库错误。
 */
int db_directory_exists(db_handle_t* db,
                        const char* username,
                        const char* dir_path);

int db_mark_file_deleted(db_handle_t* db,
                         const char* username,
                         const char* parent_path,
                         const char* display_name);

int db_is_directory_empty(db_handle_t* db,
                          const char* username,
                          const char* dir_path,
                          int* is_empty);

int db_mark_directory_deleted(db_handle_t* db,
                              const char* username,
                              const char* dir_path);

#endif // SERVER_MODULES_DB_H
