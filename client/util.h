#ifndef NETDISK_UTIL_H
#define NETDISK_UTIL_H

#include "client.h"

// 检查用户是否登录，登录返回SIGNIN，否则返回SIGNOUT。
int check_login(user_t user_status);

// 用户输入的回显，并不会回显地获取密码。
void get_password(char* password, int max_len);

#endif //NETDISK_UTIL_H
