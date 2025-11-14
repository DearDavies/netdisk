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
#include "modules/db.h"
#include "modules/register.h"
#include "modules/login.h"

//#define BUFFERMAX 1024

/*
 * 工作线程主函数：
 * - 阻塞接收客户端的 send_message_t
 * - 解析 username/pwd/paras
 * - 根据 order_type 分发至对应模块
 */
int do_work(int client_fd, const char* base_path) {
    // 每个连接，循环处理多条请求，直到对端关闭
    send_message_t recv_send_message = {0};
    while (1) {
        // 阻塞读取一条完整的 send_message_t
        LOG_DEBUG("服务端阻塞在这");
        if (recv(client_fd, &recv_send_message, sizeof(recv_send_message), MSG_WAITALL) == 0) {
            LOG_INFO("客户端断开");
            return -1;
        }
        LOG_DEBUG("服务端读取到了一条消息");

        // 从 paras 中解析三元组：username/pwd/paras
        char username[256] = {0};
        char pwd[PATH_MAX] = {0};
        char arg[PATH_MAX] = {0};
        LOG_DEBUG("接收到的ordertype = %d", recv_send_message.order_type);
        parse_kv(recv_send_message.paras, "username", username, sizeof(username));
        LOG_DEBUG("username = %s", username);
        parse_kv(recv_send_message.paras, "pwd", pwd, sizeof(pwd));
        LOG_DEBUG("pwd = %s", pwd);
        parse_kv(recv_send_message.paras, "paras", arg, sizeof(arg));
        LOG_DEBUG("arg = %s", arg);
        // PUT/GET 还会使用额外键：filename/size
        char filename[256] = {0};
        char size_str[64] = {0};
        char file_hash[129] = {0};
        char confirm_flag[16] = {0};
        parse_kv(recv_send_message.paras, "filename", filename, sizeof(filename));
        LOG_DEBUG("filename = %s", filename);
        parse_kv(recv_send_message.paras, "size", size_str, sizeof(size_str));
        LOG_DEBUG("size = %s", size_str);
        parse_kv(recv_send_message.paras, "hash", file_hash, sizeof(file_hash));
        LOG_DEBUG("hash = %s", file_hash);
        parse_kv(recv_send_message.paras, "confirm", confirm_flag, sizeof(confirm_flag));
        LOG_DEBUG("confirm = %s", confirm_flag);
        // 根据指令类型分发到具体模块
        switch (recv_send_message.order_type) {
            case REGISTER: {
                // 注册请求：解析 username 和 password_hash
                char username[256] = {0};
                char password_hash[129] = {0};
                parse_kv(recv_send_message.paras, "username", username, sizeof(username));
                parse_kv(recv_send_message.paras, "password", password_hash, sizeof(password_hash));
                LOG_DEBUG("来了一条注册信息：用户名是：%s", username);
                
                // 初始化数据库连接（可以优化为全局共享）
                db_handle_t db = {0};
                if (db_init(&db) == 0) {
                    db_ensure_table(&db);
                    modules_register_handle(client_fd, username, password_hash, &db);
                    db_close(&db);
                } else {
                    send_kv_response(client_fd, "result=fail&error=database connection failed");
                }
                break;
            }
            case LOGIN: {
                // 登录请求：解析 username 和 password_hash
                char login_username[256] = {0};
                char password_hash[129] = {0};
                parse_kv(recv_send_message.paras, "username", login_username, sizeof(login_username));
                parse_kv(recv_send_message.paras, "password", password_hash, sizeof(password_hash));
                
                // 初始化数据库连接（可以优化为全局共享）
                db_handle_t db = {0};
                if (db_init(&db) == 0) {
                    db_ensure_table(&db);
                    modules_login_handle(client_fd, login_username, password_hash, &db);
                    db_close(&db);
                } else {
                    send_kv_response(client_fd, "result=fail&error=database connection failed");
                }
                break;
            }
            case CD: {
                db_handle_t db = {0};
                if (db_init(&db) == 0) {
                    if (db_ensure_table(&db) == 0) {
                        modules_cd_handle(client_fd, base_path, username, pwd, arg, &db);
                    } else {
                        send_kv_response(client_fd, "result=fail&error=init table failed");
                    }
                    db_close(&db);
                } else {
                    send_kv_response(client_fd, "result=fail&error=database connect failed");
                }
                break;
            }
            case LS:
            {
                db_handle_t db = {0};
                if (db_init(&db) == 0) {
                    if (db_ensure_table(&db) == 0) {
                        modules_ls_handle(client_fd, base_path, username, pwd, &db);
                    } else {
                        send_kv_response(client_fd, "result=fail&error=init table failed");
                    }
                    db_close(&db);
                } else {
                    send_kv_response(client_fd, "result=fail&error=database connect failed");
                }
                break;
            }
            case MKDIR: {
                db_handle_t db = {0};
                if (db_init(&db) == 0) {
                    if (db_ensure_table(&db) == 0) {
                        modules_mkdir_handle(client_fd, base_path, username, pwd, arg, &db);
                    } else {
                        send_kv_response(client_fd, "result=fail&error=init table failed");
                    }
                    db_close(&db);
                } else {
                    send_kv_response(client_fd, "result=fail&error=database connect failed");
                }
                break;
            }
            case RM: {
                db_handle_t db = {0};
                if (db_init(&db) == 0) {
                    if (db_ensure_table(&db) == 0) {
                        modules_rm_handle(client_fd, base_path, username, pwd, arg, confirm_flag, &db);
                    } else {
                        send_kv_response(client_fd, "result=fail&error=init table failed");
                    }
                    db_close(&db);
                } else {
                    send_kv_response(client_fd, "result=fail&error=database connect failed");
                }
                break;
            }
            case PUT: {
                db_handle_t db = {0};
                if (db_init(&db) == 0) {
                    if (db_ensure_table(&db) == 0) {
                        modules_put_handle(client_fd, base_path, username, pwd, arg, filename, size_str, file_hash, &db);
                    } else {
                        send_kv_response(client_fd, "result=fail&error=init table failed");
                    }
                    db_close(&db);
                } else {
                    send_kv_response(client_fd, "result=fail&error=database connect failed");
                }
                break;
            }
            case GET: {
                db_handle_t db = {0};
                if (db_init(&db) == 0) {
                    if (db_ensure_table(&db) == 0) {
                        modules_get_handle(client_fd, base_path, username, pwd, arg, &db);
                    } else {
                        send_kv_response(client_fd, "result=fail&error=init table failed");
                    }
                    db_close(&db);
                } else {
                    send_kv_response(client_fd, "result=fail&error=database connect failed");
                }
                break;
            }
            default:
                LOG_INFO("客户端发来一个无效命令");
        }
    }

    return 0; // 不会到达
}
