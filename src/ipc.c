#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "ipc.h"
#include "handle-plugin.h"

const char *ipc_socket_file = "/tmp/buzzay.sock";

int ipc_send_msg(char *msg) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, ipc_socket_file, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        return 1;
    }

    // Send the command string
    write(fd, msg, strlen(msg));

    printf("OK!\n");
    
    close(fd);
    return 0;
}

int handle_ipc_connection(int fd, uint32_t mask, void *data) {
    struct buzzay_server *server = (struct buzzay_server *)data;

    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) return 0;

    char buffer[1024];
    ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        char *token;

        token = strtok(buffer, " ");
        if (strcmp(token, "load") == 0) {
            token = strtok(NULL, " ");
            const char *plugin = token;

            printf("Loading plugin: %s\n", plugin);
            char *path = resolve_plugin_path(plugin);
            if (path == NULL) {
                printf("Path of plugin could not be resolved.\n");
                close(client_fd);
                return 1;
            }

            printf("Resolved plugin path: %s\n", path);
            handle_plugin(path, plugin, server);
        } else if (strcmp(token, "msg") == 0) {
            token = strtok(NULL, " ");
            const char *plugin_name = token;

            char *margv[100];
            int i = 0;

            token = strtok(NULL, " ");
            while (token != NULL && i < 99) {
                margv[i] = token;
                i++;
                token = strtok(NULL, " ");
            }

            // null terminating
            margv[i] = NULL;
            msg_plugin(plugin_name, i, margv);
        }
    }

    close(client_fd);
    return 0;
}
