#include "network.h"
#include "chat.h"

#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

#define BACKLOG 1

static void die(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

void start_server(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        die("socket");

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        close(server_fd);
        die("setsockopt");
    }

    struct sockaddr_in client_address = {0};
    struct sockaddr_in server_address = {0};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        close(server_fd);
        die("bind");
    }

    if (listen(server_fd, BACKLOG) < 0)
    {
        close(server_fd);
        die("listen");
    }

    printf("Chat created.\n");
    printf("Waiting for client...\n");

    socklen_t client_address_len = sizeof(client_address);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len);
    if (client_fd < 0)
    {
        close(server_fd);
        die("accept");
    }

    printf("Client connected!\n\n");

    run_chat(client_fd);

    close(client_fd);
    close(server_fd);
}

void start_client(const struct in_addr *addr, int port)
{
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
        die("socket");

    struct sockaddr_in server_address = {0};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr = *addr;

    if (connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        close(client_fd);
        die("connect");
    }

    printf("Successfully connected!\n\n");

    run_chat(client_fd);

    close(client_fd);
}
