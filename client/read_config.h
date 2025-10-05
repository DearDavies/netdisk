#ifndef NETDISK_READ_CONFIG_H
#define NETDISK_READ_CONFIG_H

#define MAX_LINE_LENGTH 256

// 结构体：存储键值对
typedef struct KeyValue {
    char *key;
    char *value;
    struct KeyValue *next;
} KeyValue;

// 结构体：存储节 (Section)
typedef struct Section {
    char *name;
    KeyValue *pairs;
    struct Section *next;
} Section;

// 去除字符串首尾的空白字符 (in-place)
char* trim_whitespace(char *str);

// 释放所有配置占用的内存
void free_config(Section *head);

// 解析INI文件
Section* parse_ini_file(const char *filename);

// 根据节名和键名获取配置值
const char* get_config_value(const Section *head, const char *section_name, const char *key_name);

#endif //NETDISK_READ_CONFIG_H