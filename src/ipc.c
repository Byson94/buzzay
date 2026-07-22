#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "ipc.h"
#include "server.h"
#include "handle-plugin.h"
#include "macro-utils.h"

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

    char buffer[1024];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        char *token;

        token = strtok(buffer, " ");
        if (token != NULL && strcmp(token, "response") == 0) {
            token = strtok(NULL, " ");
            while (token != NULL) {
                printf("%s ", token);
                token = strtok(NULL, " ");
            }
            printf("\n");
        }
    }

    close(fd);
    return 0;
}

void send_msg_back(int client_fd, const char* msg) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "response %s", msg);
    write(client_fd, buffer, strlen(buffer));
}

int handle_ipc_connection(int fd, uint32_t mask, void *data) {
    UNUSED(mask);

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

            // reexecute the plugin if it already exists
            if (handle_if_plugin_exists(plugin)) {
                close(client_fd);
                return 0;
            }

            char *path = resolve_plugin_path(plugin);
            if (path == NULL) {
                send_msg_back(client_fd, "Path of plugin could not be resolved.\n");
                close(client_fd);
                return 1;
            }

            printf("Resolved plugin path: %s\n", path);
            handle_plugin(path, plugin, server, client_fd);
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
            msg_plugin(plugin_name, i, margv, client_fd);
        }
    }

    close(client_fd);
    return 0;
}
