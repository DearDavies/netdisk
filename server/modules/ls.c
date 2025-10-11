#include "ls.h"
#include "common.h"

#include <dirent.h>
#include <string.h>

/*
 * LS：拼接真实目录并遍历，过滤 '.' 和 '..'，以换行拼接返回。
 */
void modules_ls_handle(int client_fd, const char* base_path, const char* username, const char* pwd) {
    // 计算当前逻辑目录对应的真实路径
    char abs_path[4096] = {0};
    normalize_join_path(base_path, username, pwd, "", abs_path, sizeof(abs_path), NULL, 0);

    // 打开目录失败则返回错误说明
    DIR* dir = opendir(abs_path);
    if (!dir) {
        char buf[256];
        snprintf(buf, sizeof(buf), "result=%s", "open dir failed");
        send_kv_response(client_fd, buf);
        return;
    }

    // 逐条读取目录项，忽略 . 与 ..，以换行拼接
    char out[8192] = {0};
    size_t pos = 0;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        size_t l = strlen(ent->d_name);
        if (pos + l + 1 >= sizeof(out)) break; // 防止缓冲区溢出
        memcpy(out + pos, ent->d_name, l);
        pos += l;
        out[pos++] = '\n';
    }
    closedir(dir);
    out[pos] = '\0';

    // 封装为 result=... 返回
    char kv[8500] = {0};
    snprintf(kv, sizeof(kv), "result=%s", out);
    send_kv_response(client_fd, kv);
}


