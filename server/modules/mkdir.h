#ifndef SERVER_MODULES_MKDIR_H
#define SERVER_MODULES_MKDIR_H

/*
 * 处理 MKDIR 命令：在当前逻辑目录或绝对逻辑路径处创建目录。
 * 成功："result=ok"；失败："result=<原因>"。
 */
void modules_mkdir_handle(int client_fd, const char* base_path, const char* username, const char* pwd, const char* arg);

#endif // SERVER_MODULES_MKDIR_H

