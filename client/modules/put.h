#ifndef NETDISK_PUT_H
#define NETDISK_PUT_H

#include "../client.h"

void modules_put(order_t instruction, user_t* user_status, int sock_fd);

#endif //NETDISK_PUT_H