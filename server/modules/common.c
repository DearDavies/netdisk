#include "common.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <limits.h>

/*
 * 发送“长度前缀 + 文本”的响应。长度为 4 字节网络序，文本不含终止符。
 */
void send_kv_response(int client_fd, const char* kv_text) {
    /*
     * 这里必须显式使用 uint32_t（固定 4 字节）携带长度，保持协议在不同平台上一致。
     * 若用 size_t（在 64 位环境常为 8 字节）将导致客户端读取长度时出现错位，从而卡住或解析失败。
     */
    uint32_t len = (uint32_t)strlen(kv_text);
    uint32_t net_len = htonl(len);
    // 先发送长度前缀
    send(client_fd, &net_len, sizeof(net_len), MSG_NOSIGNAL);
    // 再发送正文（不包含终止符）
    if (len > 0) {
        send(client_fd, kv_text, len, MSG_NOSIGNAL);
    }
}

/*
 * 解析简单的 key=value&key2=value2 字符串，提取指定 key。
 */
int parse_kv(const char* kv, const char* key, char* out, size_t out_sz) {
    size_t key_len = strlen(key);
    const char* p = kv;
    while (p && *p) {
        // 扫描 key 段 [kstart, kend)
        const char* kstart = p;
        while (*p && *p != '=' && *p != '&') p++;
        const char* kend = p;
        // 命中目标 key，且后续应有 '='
        if ((size_t)(kend - kstart) == key_len && strncmp(kstart, key, key_len) == 0 && *p == '=') {
            p++; // 跳过 '='，进入 value 段
            const char* vstart = p;
            while (*p && *p != '&') p++;
            size_t vlen = (size_t)(p - vstart);
            // 拷贝受 out_sz 限制；结尾补 0
            if (vlen >= out_sz) vlen = out_sz - 1;
            memcpy(out, vstart, vlen);
            out[vlen] = '\0';
            return 0;
        }
        // 跳过当前对，进入下一对
        while (*p && *p != '&') p++;
        if (*p == '&') p++;
    }
    if (out_sz > 0) out[0] = '\0';
    return -1;
}

/*
 * 规范化逻辑路径并拼接真实路径：
 * - add 以 '/' 开头：基于根（忽略 pwd）；否则追加到 pwd
 * - 处理 '.' 和 '..'，拒绝越界（'..' 到根即止）
 * - out_pwd 输出规范后的逻辑 pwd，始终以 '/' 开头
 * - out_abs 输出 base_path/username + out_pwd 的真实路径
 */
void normalize_join_path(const char* base_path,
                         const char* username,
                         const char* pwd,
                         const char* add,
                         char* out_abs,
                         size_t out_sz,
                         char* out_pwd,
                         size_t out_pwd_sz) {
    // 先合成逻辑路径 combined：基于根（add 以 '/' 开头）或基于 pwd 追加
    char combined[PATH_MAX] = {0};
    if (add && add[0] == '/') {
        snprintf(combined, sizeof(combined), "%s", add);
    } else if (add && add[0] != '\0') {
        if (pwd && strlen(pwd) > 1) {
            snprintf(combined, sizeof(combined), "%s/%s", pwd, add);
        } else {
            snprintf(combined, sizeof(combined), "/%s", add);
        }
    } else {
        snprintf(combined, sizeof(combined), "%s", pwd ? pwd : "/");
    }

    // 使用栈规范化：处理 '.' 和 '..'
    char clean_pwd[PATH_MAX] = {0};
    char* stack[PATH_MAX];
    int top = 0;
    char temp[PATH_MAX] = {0};
    strncpy(temp, combined, sizeof(temp) - 1);
    char* token = strtok(temp, "/");
    while (token) {
        if (strcmp(token, ".") == 0 || strcmp(token, "") == 0) {
            // 当前目录，跳过
        } else if (strcmp(token, "..") == 0) {
            if (top > 0) top--;
        } else {
            stack[top++] = token;
        }
        token = strtok(NULL, "/");
    }
    // 重新拼接 clean_pwd，保证以 '/' 开头
    size_t pos = 0;
    clean_pwd[pos++] = '/';
    for (int i = 0; i < top; i++) {
        size_t l = strlen(stack[i]);
        if (pos + l + 1 >= sizeof(clean_pwd)) break;
        memcpy(clean_pwd + pos, stack[i], l);
        pos += l;
        if (i != top - 1) clean_pwd[pos++] = '/';
    }
    clean_pwd[pos] = '\0';
    if (top == 0) strcpy(clean_pwd, "/");

    if (out_pwd && out_pwd_sz > 0) {
        // 输出规范化后的逻辑路径
        strncpy(out_pwd, clean_pwd, out_pwd_sz - 1);
        out_pwd[out_pwd_sz - 1] = '\0';
    }
    if (out_abs && out_sz > 0) {
        // 拼接真实文件系统路径：base/username + clean_pwd
        snprintf(out_abs, out_sz, "%s/%s%s", base_path, username ? username : "", clean_pwd);
    }
}

/* 返回 p 是否存在且为目录。*/
int is_dir(const char* p) {
    struct stat st; if (stat(p, &st) != 0) return 0; return S_ISDIR(st.st_mode);
}

void split_parent_and_name(const char* logical_path,
                           char* parent_out,
                           size_t parent_sz,
                           char* name_out,
                           size_t name_sz) {
    if (!logical_path || logical_path[0] == '\0') {
        if (parent_out && parent_sz > 0) {
            strncpy(parent_out, "/", parent_sz - 1);
            parent_out[parent_sz - 1] = '\0';
        }
        if (name_out && name_sz > 0) {
            name_out[0] = '\0';
        }
        return;
    }
    const char* last_slash = strrchr(logical_path, '/');
    if (!last_slash) {
        if (parent_out && parent_sz > 0) {
            strncpy(parent_out, "/", parent_sz - 1);
            parent_out[parent_sz - 1] = '\0';
        }
        if (name_out && name_sz > 0) {
            strncpy(name_out, logical_path, name_sz - 1);
            name_out[name_sz - 1] = '\0';
        }
        return;
    }
    if (parent_out && parent_sz > 0) {
        if (last_slash == logical_path) {
            strncpy(parent_out, "/", parent_sz - 1);
            parent_out[parent_sz - 1] = '\0';
        } else {
            size_t parent_len = (size_t)(last_slash - logical_path);
            if (parent_len >= parent_sz) parent_len = parent_sz - 1;
            memcpy(parent_out, logical_path, parent_len);
            parent_out[parent_len] = '\0';
        }
    }
    if (name_out && name_sz > 0) {
        strncpy(name_out, last_slash + 1, name_sz - 1);
        name_out[name_sz - 1] = '\0';
    }
}

void build_logical_path(const char* pwd,
                        const char* add,
                        char* out,
                        size_t out_sz) {
    char dummy[PATH_MAX] = {0};
    const char* safe_pwd = (pwd && pwd[0]) ? pwd : "/";
    normalize_join_path("", "", safe_pwd, add, dummy, sizeof(dummy), out, out_sz);
}
