#include "ls.h"
#include "common.h"
#include "db.h"
#include "../logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * LS：基于数据库中的 files 元数据列出逻辑目录。
 * 1. 通过 db_list_entries_by_path 查询指定用户 parent_path 下的所有条目；
 * 2. 将目录排在文件之前，并按名称排序（SQL 内已完成）；
 * 3. 仅返回逻辑名称；目录追加一个 '/' 以便前端识别。
 */
void modules_ls_handle(int client_fd,
                       const char* base_path,
                       const char* username,
                       const char* pwd,
                       db_handle_t* db) {
    (void)base_path;  // 新存储模型下不再直接遍历物理目录，保留参数用于兼容

    if (!db || !db->conn || !username) {
        send_kv_response(client_fd, "result=fail&error=database not ready");
        return;
    }

    const char* parent_path = (pwd && pwd[0]) ? pwd : "/";
    db_file_entry_t* entries = NULL;
    size_t entry_count = 0;
    if (db_list_entries_by_path(db, username, parent_path, &entries, &entry_count) != 0) {
        LOG_ERROR("从数据库列出目录失败：username=%s, path=%s", username, parent_path);
        send_kv_response(client_fd, "result=fail&error=list directory failed");
        return;
    }

    // 预估返回缓冲区大小：每条名称最多 255 字符 + '/' + '\n'
    size_t buf_cap = entry_count ? (entry_count * 270) : 1;
    char* listing = (char*)calloc(buf_cap, sizeof(char));
    if (!listing) {
        db_free_entries(entries);
        send_kv_response(client_fd, "result=fail&error=out of memory");
        return;
    }

    size_t used = 0;
    for (size_t i = 0; i < entry_count; i++) {
        const char* name = entries[i].name;
        size_t name_len = strlen(name);
        size_t extra = entries[i].is_dir ? 2 : 1; // '/' + '\n' or '\n'
        // 需要确保 listing 缓冲区足够，必要时扩容
        while (used + name_len + extra >= buf_cap) {
            size_t new_cap = buf_cap * 2;
            char* tmp = (char*)realloc(listing, new_cap);
            if (!tmp) {
                free(listing);
                db_free_entries(entries);
                send_kv_response(client_fd, "result=fail&error=out of memory");
                return;
            }
            listing = tmp;
            memset(listing + buf_cap, 0, new_cap - buf_cap);
            buf_cap = new_cap;
        }
        memcpy(listing + used, name, name_len);
        used += name_len;
        if (entries[i].is_dir) {
            listing[used++] = '/';
        }
        listing[used++] = '\n';
    }
    if (used == 0) {
        listing[0] = '\0';
    } else if (used < buf_cap) {
        listing[used] = '\0';
    } else {
        listing[buf_cap - 1] = '\0';
    }

    size_t kv_len = strlen("result=") + strlen(listing) + 1;
    char* kv_buf = (char*)malloc(kv_len);
    if (!kv_buf) {
        free(listing);
        db_free_entries(entries);
        send_kv_response(client_fd, "result=fail&error=out of memory");
        return;
    }
    snprintf(kv_buf, kv_len, "result=%s", listing);
    send_kv_response(client_fd, kv_buf);

    free(kv_buf);
    free(listing);
    db_free_entries(entries);
}

