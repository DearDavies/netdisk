#ifndef WORK_H
#define WORK_H

typedef struct {
    int order_type;
    char paras[4097];
} send_message_t;

typedef enum {
    INVALID,
    EMPTY,
    CD,
    MKDIR,
    PUT,
    GET,
    LS,
    RM,
    PWD
} order_type_t;

/*
 * 功能：子线程要做的工作。
 * 参数：客户端的连接。
 * 返回值：成功返回 0，失败返回非零值。
 */
int do_work(int client_fd, const char* base_path);

#endif //WORK_H
