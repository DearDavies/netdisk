#ifndef SERVER_MODULES_RM_H
#define SERVER_MODULES_RM_H

/*
 * 处理 RM 命令：删除文件或空目录（不做递归）。
 * 成功："result=ok"；失败："result=rm failed"。
 */
void modules_rm_handle(int client_fd, const char* base_path, const char* username, const char* pwd, const char* arg);

#endif // SERVER_MODULES_RM_H

