#include "subroutine.h"
#include "util.h"
#include "MACRO.h"
#include "logger.h"
#include "read_config.h"
#include "client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "modules/cd.h"
#include "modules/get.h"
#include "modules/ls.h"
#include "modules/mkdir.h"
#include "modules/put.h"
#include "modules/pwd.h"
#include "modules/rm.h"
#include <ctype.h>

/*
 * 接收服务端的"长度前缀 + 文本"响应，返回 malloc 的字符串（需调用方 free）。
 */
static char* recv_kv_response_alloc(int sock_fd) {
    uint32_t net_len = 0;
    if (recv(sock_fd, &net_len, sizeof(net_len), MSG_WAITALL) == 0) {
        return NULL;
    }
    uint32_t len = ntohl(net_len);
    char* s = (char*)calloc(len + 1, sizeof(char));
    if (!s) return NULL;
    if (len > 0) {
        if (recv(sock_fd, s, len, MSG_WAITALL) == 0) {
            free(s);
            return NULL;
        }
    }
    return s;
}

/*
 * 从形如 key1=val1&key2=val2 的响应中解析 key 的值。
 */
static int parse_kv_local(const char* kv, const char* key, char* out, size_t out_sz) {
    size_t key_len = strlen(key);
    const char* p = kv;
    while (p && *p) {
        const char* kstart = p;
        while (*p && *p != '=' && *p != '&') p++;
        const char* kend = p;
        if ((size_t)(kend - kstart) == key_len && strncmp(kstart, key, key_len) == 0 && *p == '=') {
            p++;
            const char* vstart = p;
            while (*p && *p != '&') p++;
            size_t vlen = (size_t)(p - vstart);
            if (vlen >= out_sz) vlen = out_sz - 1;
            memcpy(out, vstart, vlen);
            out[vlen] = '\0';
            return 0;
        }
        while (*p && *p != '&') p++;
        if (*p == '&') p++;
    }
    if (out_sz > 0) out[0] = '\0';
    return -1;
}

// 执行登录、注册、退出的流程。
void try_login(int choice, user_t* user_status, int sock_fd) {
    if (choice == MENU_REGISTER) {
        // 执行注册流程，打印提示
        char reg_username[100] = {0};
        char reg_password[128] = {0};
        char reg_repeat_password[128] = {0};
        // 接收输入的用户名、密码
        printf("请输入用户名：");
        fflush(stdout);
        fgets(reg_username, sizeof(reg_username) - 1, stdin);
        reg_username[strcspn(reg_username, "\n")] = '\0';

        for (int i = 0; i < 3; i++) {
            printf("请输入密码（不会回显）：");
            fflush(stdout);
            get_password(reg_password, sizeof(reg_password));
            printf("请再输入密码（不会回显）：");
            fflush(stdout);
            get_password(reg_repeat_password, sizeof(reg_repeat_password));
            if (strcmp(reg_password, reg_repeat_password) == 0) {
                break;
            }
            printf("两次输入的密码不一致！请重新输入密码（%d/3）\n", i + 1);
            if (i == 2) {
                printf("失败次数过多！将重试\n");
                return;
            }
        }

        // 检查用户名是否为空
        if (strlen(reg_username) == 0) {
            printf("用户名不能为空！\n");
            return;
        }

        // 使用 SHA512 加密密码
        char password_hash[129] = {0};
        if (sha512_hash(reg_password, password_hash, sizeof(password_hash)) != 0) {
            printf("密码加密失败！\n");
            return;
        }

        // 组织注册请求：构造 send_message_t
        send_message_t send_message = {0};
        send_message.order_type = REGISTER;
        char kv[512] = {0};
        snprintf(kv, sizeof(kv), "username=%s&password=%s", reg_username, password_hash);
        strncpy(send_message.paras, kv, sizeof(send_message.paras) - 1);

        // 发送注册请求到服务端
        if (send(sock_fd, &send_message, sizeof(send_message_t), MSG_NOSIGNAL) == -1) {
            LOG_ERROR("发送注册请求失败");
            printf("发送注册请求失败！\n");
            return;
        }

        // 接收服务端响应
        char* resp = recv_kv_response_alloc(sock_fd);
        if (!resp) {
            printf("服务端断开连接！\n");
            return;
        }

        // 解析响应结果
        char result[64] = {0};
        parse_kv_local(resp, "result", result, sizeof(result));
        
        if (strcasecmp(result, "ok") == 0) {
            printf("注册成功！\n");
            // 注册成功后，自动执行登录流程
            free(resp);
            try_login(MENU_LOGIN, user_status, sock_fd);
            return;
        } else {
            char error_msg[256] = {0};
            parse_kv_local(resp, "error", error_msg, sizeof(error_msg));
            printf("注册失败：%s\n", strlen(error_msg) > 0 ? error_msg : result);
            free(resp);
            return;
        }
    }
    if (choice == MENU_LOGIN) {
        printf("\n==================\n正在登录\n");
        char login_username[100] = {0};
        char login_password[128] = {0};
        // 接收输入的用户名、密码
        printf("请输入用户名：");
        fgets(login_username, sizeof(login_username) - 1, stdin);
        login_username[strcspn(login_username, "\n")] = '\0';

        printf("请输入密码（不会回显）：");
        fflush(stdout);
        get_password(login_password, sizeof(login_password));
        // 使用 SHA512 加密密码
        char password_hash[129] = {0};
        if (sha512_hash(login_password, password_hash, sizeof(password_hash)) != 0) {
            printf("密码加密失败！\n");
            return;
        }

        // 组织登录请求：构造 send_message_t
        send_message_t send_message = {0};
        send_message.order_type = LOGIN;
        char kv[512];
        snprintf(kv, sizeof(kv), "username=%s&password=%s", login_username, password_hash);
        strncpy(send_message.paras, kv, sizeof(send_message.paras) - 1);

        // 发送登录请求到服务端
        if (send(sock_fd, &send_message, sizeof(send_message_t), MSG_NOSIGNAL) == -1) {
            LOG_ERROR("发送登录请求失败");
            printf("发送登录请求失败！\n");
            return;
        }

        // 接收服务端响应
        char* resp = recv_kv_response_alloc(sock_fd);
        if (!resp) {
            printf("服务端断开连接！\n");
            return;
        }

        // 解析响应结果
        char result[64] = {0};
        parse_kv_local(resp, "result", result, sizeof(result));
        
        if (strcasecmp(result, "ok") == 0) {
            printf("登录成功。\n");
            // 修改存储状态
            user_status->is_login = SIGNIN;
            user_status->exit_flag = EXIT_FLAG_NO;
            user_status->username = strdup(login_username);
            user_status->my_pwd = strdup("/");
            free(resp);
        } else {
            char error_msg[256] = {0};
            parse_kv_local(resp, "error", error_msg, sizeof(error_msg));
            printf("登录失败：%s\n", strlen(error_msg) > 0 ? error_msg : result);
            free(resp);
        }
    }
    if (choice == MENU_EXIT) {
        printf("谢谢使用，再见。\n");
        user_status->exit_flag = EXIT_FLAG_YES;
    }
}

// 接收用户输入的命令
void read_user_order(char** user_order, user_t user_status) {
    if (user_order == NULL) {
        LOG_ERROR("user_order 的地址是空指针");
        return;
    }

    char buffer[1024];
    printf("%s:%s$ ", user_status.username, user_status.my_pwd);
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        LOG_ERROR("从标准输入读取用户命令错误");
        exit(EXIT_FAILURE);
    }

    // 删除指令最后的换行
    buffer[strcspn(buffer, "\n")] = '\0';

    *user_order = strdup(buffer);

    if (*user_order == NULL) {
        LOG_ERROR("strdup 分配内存错误");
    }
}

// 解析命令
void parse_user_order(const char* user_order, order_t* instruction, user_t* user_status) {
    if (user_order == NULL) {
        instruction->order_type = EMPTY;
        change_order_t_paras(instruction, NULL);
        return;
    }
    // 临时存储一份用户指令
    char* temp_user_order = strdup(user_order);
    // 去除前后的空白字符
    char* temp_trimmed_order = trim_whitespace(temp_user_order);
    // 如果用户输入“exit”（不区分大小写），则退出当前用户的登录状态
    if (strcasecmp(temp_trimmed_order, "exit") == 0) {
        user_status->exit_flag = EXIT_FLAG_NO;
        user_status->is_login = SIGNOUT;
        // 获取用户名、释放空间、置nobody
        char* temp = user_status->username;
        free(temp);
        user_status->username = "nobody";
        // 获取pwd、释放空间、置初始
        temp = user_status->my_pwd;
        free(temp);
        user_status->my_pwd = "/";
        return;
    }

    char* order_type = NULL;

    const char* temp_p = temp_trimmed_order; // 使用一个指针来遍历输入字符串

    // 提取命令
    const char* cmd_start = temp_p; // 记录命令的起始位置
    while (*temp_p && !isspace(*temp_p)) {
        temp_p++;
    }
    const char* cmd_end = temp_p; // 记录命令的结束位置
    // 拷贝命令到输出缓冲区
    size_t cmd_len = cmd_end - cmd_start;
    if (cmd_len < 0) {
        LOG_INFO("命令解析的长度非法");
        instruction->order_type = INVALID;
        return;
    }
    if (cmd_len == 0) {
        instruction->order_type = EMPTY;
        return;
    }
    order_type = (char*)calloc(cmd_len + 1, sizeof(char));
    strncpy(order_type, cmd_start, cmd_len);

    // 跳过命令和参数之间的空格
    while (*temp_p && isspace(*temp_p)) {
        temp_p++;
    }

    // 如果后面还有内容，就是参数
    if (*temp_p != '\0') {
        // 直接将剩余部分拷贝
        const char* paras = strdup(temp_p);
        change_order_t_paras(instruction, paras);
    }

    // // 从前往后，找到第一个空格的索引下标
    // size_t index = strcspn(temp_trimmed_order, " ");
    // // 将空格置为0
    // temp_trimmed_order[index] = '\0';
    // // 获取指令类型
    // order_type = strdup(temp_trimmed_order);

    if (strcasecmp(order_type, "ls") == 0) {
        instruction->order_type = LS;
        change_order_t_paras(instruction, NULL);
    }
    else if (strcasecmp(order_type, "pwd") == 0) {
        instruction->order_type = PWD;
        change_order_t_paras(instruction, NULL);
    }
    else if (strcasecmp(order_type, "cd") == 0) {
        instruction->order_type = CD;
    }
    else if (strcasecmp(order_type, "mkdir") == 0) {
        instruction->order_type = MKDIR;
    }
    else if (strcasecmp(order_type, "put") == 0) {
        instruction->order_type = PUT;
    }
    else if (strcasecmp(order_type, "get") == 0) {
        instruction->order_type = GET;
    }
    else if (strcasecmp(order_type, "rm") == 0) {
        instruction->order_type = RM;
    }
    else {
        instruction->order_type = INVALID;
        change_order_t_paras(instruction, NULL);
    }
    LOG_DEBUG("解析用户输入的命令(%s)，种类是 %s，参数是 %s", user_order, order_type, instruction->paras);
    free(temp_user_order);
}

// 根据不同的命令，执行不同流程
void dispath_order(order_t instruction, user_t* user_status, int sock_fd) {
    if (user_status->is_login == SIGNOUT) {
        return;
    }
    switch (instruction.order_type) {
        case CD:
            modules_cd(instruction, user_status, sock_fd);
            break;
        case PWD:
            modules_pwd(instruction, user_status, sock_fd);
            break;
        case LS:
            modules_ls(instruction, user_status, sock_fd);
            break;
        case MKDIR:
            modules_mkdir(instruction, user_status, sock_fd);
            break;
        case PUT:
            modules_put(instruction, user_status, sock_fd);
            break;
        case GET:
            modules_get(instruction, user_status, sock_fd);
            break;;
        case RM:
            modules_rm(instruction, user_status, sock_fd);
            break;
        case EMPTY:
            break;
        default:
            LOG_INFO("无效命令");
            break;
    }
}
