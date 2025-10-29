#include "read_config.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 去除字符串首尾的空白字符 (in-place)
char* trim_whitespace(char* str) {
    // 去除前面的空白字符
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) {
        // 全空？
        return str;
    }

    // 去除尾部的空白字符
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    // 在尾部加字符串终止符
    *(end + 1) = '\0';

    return str;
}

// 释放所有配置占用的内存
void free_config(Section* head) {
    Section* current_section = head;
    while (current_section != NULL) {
        KeyValue* current_pair = current_section->pairs;
        while (current_pair != NULL) {
            KeyValue* temp_pair = current_pair;
            current_pair = current_pair->next;
            free(temp_pair->key);
            free(temp_pair->value);
            free(temp_pair);
        }
        Section* temp_section = current_section;
        current_section = current_section->next;
        free(temp_section->name);
        free(temp_section);
    }
}

// 解析INI文件
Section* parse_ini_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR("Error opening config file");
        return NULL;
    }

    Section* head = NULL;
    Section* current_section = NULL;
    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), file)) {
        char* trimmed_line = trim_whitespace(line);

        // 忽略空行或注释
        if (strlen(trimmed_line) == 0 || trimmed_line[0] == ';' || trimmed_line[0] == '#') {
            continue;
        }

        // 检查是否为节 (Section)
        if (trimmed_line[0] == '[' && trimmed_line[strlen(trimmed_line) - 1] == ']') {
            // 创建新的 Section
            Section* new_section = (Section*)malloc(sizeof(Section));
            if (!new_section) {
                LOG_ERROR("Memory allocation failed for section.\n");
                fclose(file);
                free_config(head);
                return NULL;
            }
            // 提取节名
            char* section_name = &trimmed_line[1];
            section_name[strlen(section_name) - 1] = '\0';

            new_section->name = strdup(trim_whitespace(section_name));
            new_section->pairs = NULL;
            new_section->next = NULL;

            // 添加到链表
            if (head == NULL) {
                head = new_section;
                current_section = new_section;
            }
            else {
                current_section->next = new_section;
                current_section = new_section;
            }
        }
        // 检查是否为键值对
        else if (current_section && strchr(trimmed_line, '=')) {
            // 创建新的 KeyValue
            KeyValue* new_pair = (KeyValue*)malloc(sizeof(KeyValue));
            if (!new_pair) {
                LOG_ERROR("Memory allocation failed for key-value pair.\n");
                fclose(file);
                free_config(head);
                return NULL;
            }

            // 分割 key 和 value
            char* key = strtok(trimmed_line, "=");
            char* value = strtok(NULL, "=");

            new_pair->key = strdup(trim_whitespace(key));
            new_pair->value = strdup(value ? trim_whitespace(value) : ""); // 处理空值的情况
            new_pair->next = NULL;

            // 添加到当前节的链表
            if (current_section->pairs == NULL) {
                current_section->pairs = new_pair;
            }
            else {
                KeyValue* last_pair = current_section->pairs;
                while (last_pair->next != NULL) {
                    last_pair = last_pair->next;
                }
                last_pair->next = new_pair;
            }
        }
    }

    fclose(file);
    return head;
}

// 根据节名和键名获取配置值
const char* get_config_value(const Section* head, const char* section_name, const char* key_name) {
    const Section* current_section = head;
    while (current_section != NULL) {
        if (strcmp(current_section->name, section_name) == 0) {
            const KeyValue* current_pair = current_section->pairs;
            while (current_pair != NULL) {
                if (strcmp(current_pair->key, key_name) == 0) {
                    // 找到了Key，获取对应的Value
                    const char* value = current_pair->value;
                    // 要复制一份出去，否则会丢失
                    size_t len = strlen(value);
                    char * result = calloc(len + 1, sizeof(char));
                    strcpy(result, value);
                    return result;
                }
                current_pair = current_pair->next;
            }
        }
        current_section = current_section->next;
    }
    return NULL; // 未找到
}

// 使用示例。
// int main() {
//     // 解析配置文件
//     Section *config = parse_ini_file("config.ini");
//     if (config == NULL) {
//         return 1; // 解析失败
//     }
//
//     printf("--- Reading Configuration ---\n");
//
//     // 读取数据库配置
//     const char *db_host = get_config_value(config, "database", "host");
//     const char *db_port = get_config_value(config, "database", "port");
//     const char *db_user = get_config_value(config, "database", "user");
//     const char *db_pass = get_config_value(config, "database", "password");
//
//     printf("[database]\n");
//     printf("  Host: %s\n", db_host ? db_host : "Not Found");
//     printf("  Port: %s\n", db_port ? db_port : "Not Found");
//     printf("  User: %s\n", db_user ? db_user : "Not Found");
//     printf("  Password: %s\n", db_pass ? db_pass : "Not Found");
//     printf("\n");
//
//     // 读取服务器配置
//     const char *srv_ip = get_config_value(config, "server", "ip_address");
//     const char *srv_port = get_config_value(config, "server", "port");
//     const char *srv_https = get_config_value(config, "server", "enable_https");
//
//     printf("[server]\n");
//     printf("  IP Address: %s\n", srv_ip ? srv_ip : "Not Found");
//     printf("  Port: %s\n", srv_port ? srv_port : "Not Found");
//     printf("  Enable HTTPS: %s\n", srv_https ? srv_https : "Not Found");
//     printf("\n");
//
//     // 尝试读取一个不存在的键
//     const char *non_existent = get_config_value(config, "database", "timeout");
//     printf("Testing non-existent key 'timeout' in [database]: %s\n",
//            non_existent ? non_existent : "Correctly Not Found");
//
//
//     // 释放所有动态分配的内存
//     free_config(config);
//     printf("\nConfiguration memory freed.\n");
//
//     return 0;
// }
