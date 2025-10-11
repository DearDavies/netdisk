#ifndef SERVER_MODULES_LS_H
#define SERVER_MODULES_LS_H

/*
 * 处理 LS 命令：列出当前逻辑目录下的条目，以 \n 分隔。
 * 返回："result=<name1\nname2\n...>"
 */
void modules_ls_handle(int client_fd, const char* base_path, const char* username, const char* pwd);

#endif // SERVER_MODULES_LS_H

