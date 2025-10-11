#ifndef SERVER_MODULES_GET_H
#define SERVER_MODULES_GET_H

/*
 * 处理 GET（下载）命令：
 * 1) 客户端先发送头：order_type=GET, paras="username=...&pwd=...&paras=<remotePath>"
 * 2) 服务端响应：len + "result=ok&size=<bytes>" 或错误
 * 3) 服务端发送数据流：若干帧 4B chunk_len + data；以 0 结束
 */
void modules_get_handle(int client_fd, const char* base_path, const char* username, const char* pwd, const char* remote_path_kv);

#endif // SERVER_MODULES_GET_H

