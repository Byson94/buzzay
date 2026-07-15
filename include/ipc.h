#pragma once

#include <stdint.h>

extern const char *ipc_socket_file;

int ipc_send_msg(char *msg);
int handle_ipc_connection(int fd, uint32_t mask, void *data);
