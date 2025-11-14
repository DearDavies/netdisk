#include "util.h"
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * 检查用户是否登录：返回 SIGNIN（已登录）或 SIGNOUT（未登录）。
 */
int check_login(user_t user_status) {
    return user_status.is_login;
}

/*
 * 不显示回显地获取密码输入（使用 termios 关闭 ECHO）。
 */
void get_password(char* password, int max_len) {
    struct termios old_term, new_term;
    // 获取当前终端设置
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    // 关闭回显
    new_term.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
    // 读取密码
    if (fgets(password, max_len, stdin) != NULL) {
        // 去除末尾的换行符
        password[strcspn(password, "\n")] = '\0';
    }
    // 恢复终端设置
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
}

/*
 * 修改 user_t 结构体中的用户名。
 */
void change_user_t_username(user_t* user, const char* username) {
    if (user->username) {
        free(user->username);
    }
    user->username = username ? strdup(username) : NULL;
}

/*
 * 修改 user_t 结构体中的当前工作目录。
 */
void change_user_t_pwd(user_t* user, const char* pwd) {
    if (user->my_pwd) {
        free(user->my_pwd);
    }
    user->my_pwd = pwd ? strdup(pwd) : NULL;
}

/*
 * 修改 order_t 结构体中的参数。
 */
void change_order_t_paras(order_t* order, const char* paras) {
    if (order->paras) {
        free(order->paras);
    }
    order->paras = paras ? strdup(paras) : NULL;
}

/*
 * 使用 SHA512 对密码进行哈希加密。
 * 输入：password - 原始密码字符串
 * 输出：hash_hex - 128 字符的十六进制字符串（SHA512 输出 64 字节 = 128 个十六进制字符）
 * 返回：0 成功，-1 失败
 */
int sha512_hash(const char* password, char* hash_hex, size_t hash_hex_size) {
    if (!password || !hash_hex || hash_hex_size < 129) {
        return -1;
    }
    
    unsigned char hash[SHA512_DIGEST_LENGTH];
    SHA512_CTX ctx;
    
    // 初始化 SHA512 上下文
    if (SHA512_Init(&ctx) != 1) {
        return -1;
    }
    
    // 更新上下文，添加密码数据
    if (SHA512_Update(&ctx, password, strlen(password)) != 1) {
        return -1;
    }
    
    // 完成哈希计算
    if (SHA512_Final(hash, &ctx) != 1) {
        return -1;
    }
    
    // 将二进制哈希值转换为十六进制字符串
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
        snprintf(hash_hex + i * 2, 3, "%02x", hash[i]);
    }
    hash_hex[128] = '\0';
    
    return 0;
}

/*
 * 对指定文件的全部内容计算 SHA512 哈希，结果同样输出为 128 个十六进制字符。
 */
int sha512_file(const char* file_path, char* hash_hex, size_t hash_hex_size) {
    if (!file_path || !hash_hex || hash_hex_size < 129) {
        return -1;
    }

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    unsigned char hash[SHA512_DIGEST_LENGTH];
    unsigned char buffer[32768];
    SHA512_CTX ctx;
    if (SHA512_Init(&ctx) != 1) {
        close(fd);
        return -1;
    }

    while (1) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0) {
            close(fd);
            return -1;
        }
        if (n == 0) break;
        if (SHA512_Update(&ctx, buffer, (size_t)n) != 1) {
            close(fd);
            return -1;
        }
    }
    close(fd);

    if (SHA512_Final(hash, &ctx) != 1) {
        return -1;
    }

    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
        snprintf(hash_hex + i * 2, 3, "%02x", hash[i]);
    }
    hash_hex[128] = '\0';
    return 0;
}
