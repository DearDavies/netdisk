#ifndef SERVER_MODULES_PUT_H
#define SERVER_MODULES_PUT_H

/*
 * 处理 PUT（上传）命令：
 * 协议（基于已存在的 send_message_t 头）：
 * 1) 客户端先发送头：order_type=PUT, paras="username=...&pwd=...&paras=<remotePath>&filename=<name>&size=<bytes>"
 * 2) 服务端返回：len + "result=ok" 或错误信息
 * 3) 客户端发送数据流：若干帧，每帧为 4B chunk_len(网络序) + chunk_data；以 chunk_len==0 结束
 * 4) 服务端完成写入后返回：len + "result=ok" 或错误
 */
#include "db.h"

void modules_put_handle(int client_fd,
                        const char* base_path,
                        const char* username,
                        const char* pwd,
                        const char* remote_path_kv,
                        const char* filename_kv,
                        const char* size_kv,
                        const char* hash_kv,
                        db_handle_t* db);

#endif // SERVER_MODULES_PUT_H
