#ifndef NETDISK_UTIL_H
#define NETDISK_UTIL_H

#include "client.h"
#include <stddef.h>

// 检查用户是否登录，登录返回SIGNIN，否则返回SIGNOUT。
int check_login(user_t user_status);

// 用户输入的回显，并不会回显地获取密码。
void get_password(char* password, int max_len);

void change_user_t_username(user_t* user, const char* username);

void change_user_t_pwd(user_t* user, const char* pwd);

void change_order_t_paras(order_t* order, const char* paras);

// 使用 SHA512 对密码进行哈希加密，输出为 128 字符的十六进制字符串
int sha512_hash(const char* password, char* hash_hex, size_t hash_hex_size);

#endif //NETDISK_UTIL_H
