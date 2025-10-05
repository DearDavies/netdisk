//
// Created by deda on 2025/10/3.
//

#include "logger.h"
#include "read_config.h"
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <stdarg.h>



// --- 内部状态变量 ---
// 一个全局变量来存储从配置中读取的最低日志级别，默认级别为INFO
static LogLevel g_min_log_level = LOG_LEVEL_INFO;

// 文件句柄数组，对应 LogLevel 枚举的顺序
static FILE* log_files[4] = {NULL};

// 日志文件名
static const char* log_filenames[] = {
    "error.log",
    "warning.log",
    "info.log",
    "debug.log"
};

// 日志级别字符串表示
static const char* level_strings[] = {
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"
};

// 用于保证线程安全的互斥锁
static pthread_mutex_t log_mutex;

// --- 函数实现 ---

// 从字符串转换到LogLevel枚举
static LogLevel level_from_string(const char *level_str) {
    if (level_str == NULL) return LOG_LEVEL_INFO; // 默认值
    if (strcasecmp(level_str, "ERROR") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(level_str, "WARNING") == 0) return LOG_LEVEL_WARNING;
    if (strcasecmp(level_str, "INFO") == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(level_str, "DEBUG") == 0) return LOG_LEVEL_DEBUG;
    return LOG_LEVEL_INFO; // 对于无效输入，返回一个安全的默认值
}

// Getter函数，供宏调用
LogLevel get_min_log_level() {
    return g_min_log_level;
}

int log_init(const char *config_filename) {
    // 1. 解析配置文件并设置日志级别
    Section *config = parse_ini_file(config_filename);
    if (config) {
        const char *level_str = get_config_value(config, "logging", "log_level");
        if (level_str) {
            g_min_log_level = level_from_string(level_str);
            printf("Logger 初始化成功。Log level 从配置文件中读取为 '%s' 。\n", level_strings[g_min_log_level]);
        } else {
            printf("Logger 初始化成功。未设置'log_level' ，设置为默认值 '%s'.\n", level_strings[g_min_log_level]);
        }
        free_config(config); // 解析完就释放内存
    } else {
        fprintf(stderr, "Warning: 读取配置文件错误 '%s'. Logger 恢复默认值为 '%s'.\n",
                config_filename, level_strings[g_min_log_level]);
    }

    // 2. 初始化互斥锁和文件句柄
    if (pthread_mutex_init(&log_mutex, NULL) != 0) {
        perror("初始化 mutex 失败");
        return -1;
    }

    for (int i = 0; i <= g_min_log_level; ++i) { // 优化：只打开需要的日志文件
        log_files[i] = fopen(log_filenames[i], "a");
        if (log_files[i] == NULL) {
            perror("打开 log 文件失败");
            for (int j = 0; j < i; ++j) fclose(log_files[j]);
            pthread_mutex_destroy(&log_mutex);
            return -1;
        }
    }
    return 0;
}

void log_cleanup() {
    // 关闭所有日志文件
    for (int i = 0; i < 4; ++i) {
        if (log_files[i] != NULL) {
            fclose(log_files[i]);
        }
    }
    // 销毁互斥锁
    pthread_mutex_destroy(&log_mutex);
}

void log_message(LogLevel level, const char *file, int line, const char *func, const char *format, ...) {
    // 检查日志级别是否有效
    if (level < LOG_LEVEL_ERROR || level > LOG_LEVEL_DEBUG) {
        return;
    }

    // 获取当前时间
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", local_time);

    // 加锁以保证线程安全
    pthread_mutex_lock(&log_mutex);

    // 选择对应的日志文件
    FILE *log_file = log_files[level];
    if (log_file == NULL) {
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    // 写入日志元数据：[时间] [级别] [文件名:行号:函数名]
    fprintf(log_file, "[%s] [%-7s] [%s:%d:%s] ",
            time_buf,
            level_strings[level],
            file,
            line,
            func);

    // 处理可变参数，写入用户日志信息
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    // 写入换行符
    fprintf(log_file, "\n");

    // 刷新文件缓冲区，确保日志立即写入磁盘
    fflush(log_file);

    // 解锁
    pthread_mutex_unlock(&log_mutex);
}