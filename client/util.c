//
// Created by deda on 2025/10/2.
//

#include "util.h"
#include "logger.h"
#include <stdio.h>
#include <unistd.h>  // 包含 read() 和 STDIN_FILENO
#include <termios.h> // 包含终端属性控制函数


// 检查用户是否登录，登录返回SIGNIN，否则返回SIGNOUT。
int check_login(user_t user_status) {
    return user_status.is_login;
}

// 用户输入的回显，并不会回显地获取密码。
void get_password(char* password, int max_len) {
    struct termios old_term, new_term;

    // 1. 获取当前终端的属性，并保存
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;

    // 2. 修改终端属性，禁用回显 (ECHO)
    new_term.c_lflag &= ~(ECHO);

    // 3. 将修改后的新属性设置给终端
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    // 4. 从标准输入读取密码
    // 这里我们使用 read 函数，因为它更底层，可以更好地配合 termios
    int i = 0;
    char ch;
    while (i < max_len - 1 && read(STDIN_FILENO, &ch, 1) > 0 && ch != '\n') {
        // 为了美观，可以打印星号*来代替输入的字符
        // 如果完全不想有任何显示，可以注释掉下面这行
        // printf("*");
        fflush(stdout); // 立即刷新输出缓冲区，显示星号
        password[i++] = ch;
    }
    password[i] = '\0';

    // 5. 恢复终端原来的属性
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
}

