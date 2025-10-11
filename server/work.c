#include "work.h"
#include "logger.h"

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>
#include "modules/common.h"
#include "modules/cd.h"
#include "modules/ls.h"
#include "modules/mkdir.h"
#include "modules/rm.h"
#include "modules/put.h"
#include "modules/get.h"

//#define BUFFERMAX 1024

/*
 * 工作线程主函数：
 * - 阻塞接收客户端的 send_message_t
 * - 解析 username/pwd/paras
 * - 根据 order_type 分发至对应模块
 */
int do_work(int client_fd, const char* base_path) {
    // 每个连接循环处理多条请求，直到对端关闭
    send_message_t recv_send_message = {0};
    while (1) {
        // 阻塞读取一条完整的 send_message_t
        if (recv(client_fd, &recv_send_message, sizeof(recv_send_message), MSG_WAITALL) == 0) {
            LOG_INFO("客户端断开");
            return -1;
        }
        // 从 paras 中解析三元组：username/pwd/paras
        char username[256] = {0};
        char pwd[PATH_MAX] = {0};
        char arg[PATH_MAX] = {0};
        parse_kv(recv_send_message.paras, "username", username, sizeof(username));
        parse_kv(recv_send_message.paras, "pwd", pwd, sizeof(pwd));
        parse_kv(recv_send_message.paras, "paras", arg, sizeof(arg));
        // PUT/GET 还会使用额外键：filename/size
        char filename[256] = {0};
        char size_str[64] = {0};
        parse_kv(recv_send_message.paras, "filename", filename, sizeof(filename));
        parse_kv(recv_send_message.paras, "size", size_str, sizeof(size_str));
        // 根据指令类型分发到具体模块
        switch (recv_send_message.order_type) {
            case CD:
                modules_cd_handle(client_fd, base_path, username, pwd, arg);
                break;
            case LS:
                modules_ls_handle(client_fd, base_path, username, pwd);
                break;
            case MKDIR:
                modules_mkdir_handle(client_fd, base_path, username, pwd, arg);
                break;
            case RM:
                modules_rm_handle(client_fd, base_path, username, pwd, arg);
                break;
            case PUT:
                modules_put_handle(client_fd, base_path, username, pwd, arg, filename, size_str);
                break;
            case GET:
                modules_get_handle(client_fd, base_path, username, pwd, arg);
                break;
            default:
                LOG_INFO("客户端发来一个无效命令");
        }
    }

    return 0; // 不会到达
}
