#ifndef NETDISK_LOGGER_H
#define NETDISK_LOGGER_H

// 定义日志级别 (顺序很重要，从高到低)
typedef enum {
    LOG_LEVEL_ERROR, // 0
    LOG_LEVEL_WARNING, // 1
    LOG_LEVEL_INFO, // 2
    LOG_LEVEL_DEBUG // 3
} LogLevel;

// --- 用户接口 ---

/**
 * @brief 初始化日志系统。
 * @param config_filename INI 配置文件的路径。
 * @return 0 表示成功, -1 表示失败。
 */
int log_init(const char* config_filename);

/**
 * @brief 获取当前设置的最低日志级别。
 */
LogLevel get_min_log_level();

/**
 * @brief 清理日志系统资源。
 * 在程序结束时调用一次。
 */
void log_cleanup();


/**
 * @brief 底层日志记录宏。
 * 在调用函数前检查日志级别。
 * 这样做效率极高，如果级别不够，log_message 函数调用和其参数求值
 * 都会被编译器完全优化掉。
 */
#define LOG(level, format, ...) \
do { \
    if (level <= get_min_log_level()) { \
    log_message(level, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__); \
    } \
} while (0)


// --- 便捷的日志宏 ---

// ##__VA_ARGS__ 是一个GCC/Clang扩展，能优雅地处理可变参数为空的情况。
// 如果你的编译器不支持，可以考虑更复杂的宏技巧或要求至少有一个可变参数。

// 错误日志
#define LOG_ERROR(format, ...)   LOG(LOG_LEVEL_ERROR,   format, ##__VA_ARGS__)
// 警告日志
#define LOG_WARNING(format, ...) LOG(LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
// 信息日志
#define LOG_INFO(format, ...)    LOG(LOG_LEVEL_INFO,    format, ##__VA_ARGS__)
// 调试日志
#define LOG_DEBUG(format, ...)   LOG(LOG_LEVEL_DEBUG,   format, ##__VA_ARGS__)


/**
 * @brief 实际执行日志写入的函数（宏的后端实现）。
 * * @param level 日志级别
 * @param file 源代码文件名 (__FILE__)
 * @param line 代码行号 (__LINE__)
 * @param func 函数名 (__func__)
 * @param format 格式化字符串
 * @param ... 参数
 */
void log_message(LogLevel level, const char* file, int line, const char* func, const char* format, ...);

#endif //NETDISK_LOGGER_H
