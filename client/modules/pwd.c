#include "pwd.h"

#include <stdio.h>
#include "../util.h"

void modules_pwd(order_t instruction, user_t* user_status, int sock_fd) {
    printf("%s\n", user_status->my_pwd);
    printf("%s:%s$ ", user_status->username, user_status->my_pwd);
}