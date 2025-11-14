#ifndef SERVER_MODULES_COMMON_H
#define SERVER_MODULES_COMMON_H

#include <stddef.h>

/*
 * 以“长度前缀 + 文本”的形式向客户端回包。
 * 文本采用 key=value&key2=value2 形式，满足现有客户端解析逻辑。
 */
void send_kv_response(int client_fd, const char* kv_text);

/*
 * 从形如 key1=val1&key2=val2 的字符串中解析指定 key 的值。
 * out 至少留 1 字节（结尾 0），未找到时返回 -1。
 */
int parse_kv(const char* kv, const char* key, char* out, size_t out_sz);

/*
 * 将逻辑路径拼接到用户根目录之下，并进行规范化（处理 '.', '..'）。
 * - base_path: 服务端根（配置项）
 * - username:  用户名，用于隔离到 base_path/username
 * - pwd:       当前工作目录（以 '/' 开头，位于用户根内）
 * - add:       追加路径。以 '/' 开头时从根算，否则相对 pwd
 * - out_abs:   输出的真实文件系统绝对路径 base_path/username + clean_pwd
 * - out_pwd:   输出规范化后的逻辑路径（以 '/' 开头），可能为空
 */
void normalize_join_path(const char* base_path,
                         const char* username,
                         const char* pwd,
                         const char* add,
                         char* out_abs,
                         size_t out_sz,
                         char* out_pwd,
                         size_t out_pwd_sz);

/* 简单判断路径是否为目录（存在且为目录时返回非 0）。*/
int is_dir(const char* p);

/*
 * 将绝对逻辑路径拆分为“父目录 + 当前名称”。
 * 例如：/docs/a.txt -> parent_out=/docs, name_out=a.txt
 */
void split_parent_and_name(const char* logical_path,
                           char* parent_out,
                           size_t parent_sz,
                           char* name_out,
                           size_t name_sz);

/*
 * 仅基于逻辑路径规则（不依赖物理目录）拼接 pwd 与 add，返回规范化后的绝对逻辑路径。
 */
void build_logical_path(const char* pwd,
                        const char* add,
                        char* out,
                        size_t out_sz);

#endif // SERVER_MODULES_COMMON_H
