#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 8082

void start_server(int port)
{
    char host_chat_buffer[2048] = "Chat started!\n\n";
    size_t buffer_size = sizeof(host_chat_buffer);

    int server_fd, client_fd;
    int check_res;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("\nCHAT HOST ERROR: socket() call failed\n");
        exit(1);
    }

    struct sockaddr_in client_address;
    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    socklen_t server_address_len = sizeof(server_address);
    check_res = bind(server_fd, (struct sockaddr *)&server_address, server_address_len);
    if (check_res < 0)
    {
        perror("\nCHAT HOST ERROR: bind() call failed\n");
        exit(1);
    }

    check_res = listen(server_fd, 5);
    if (check_res < 0)
    {
        perror("\nCHAT HOST ERROR: listen() call failed\n");
        exit(1);
    }

    socklen_t client_address_len = sizeof(client_address);
    client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len);
    if (client_fd < 0)
    {
        perror("\nCHAT HOST ERROR: accept() call failed\n");
        close(server_fd);
        exit(1);
    }

    printf("Client connected!\n\n");

    ssize_t bytes_sent;
    ssize_t bytes_read;
    int keep_running_chat = 1;
    while (keep_running_chat)
    {
        bytes_sent = send(client_fd, host_chat_buffer, strlen(host_chat_buffer), 0);
        if (bytes_sent < 0)
        {
            printf("\nMessage send error. Try again: \t");
            continue;
        }

        bytes_read = recv(client_fd, host_chat_buffer, buffer_size - 1, 0);
        if (bytes_read < 1)
        {
            printf("\nMessage read error.\t");
            break;
        }
        host_chat_buffer[bytes_read] = '\0';

        printf("User: %s", host_chat_buffer);
        printf("You: ");

        if (fgets(host_chat_buffer, buffer_size, stdin) == NULL)
        {
            perror("\nCHAT HOST ERROR: message enter error\n");
            close(client_fd);
            close(server_fd);
            exit(1);
        }
    }

    close(client_fd);
    close(server_fd);
}

void start_client(const char *ip_addr, int port)
{
    char client_chat_buffer[2048];
    size_t buffer_size = sizeof(client_chat_buffer);

    int client_fd;

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
    {
        perror("\nCHAT CLIENT ERROR: socket() call failed\n");
        exit(1);
    }

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_addr, &server_address.sin_addr) < 1)
    {
        perror("\nInvalid IP address\n");
        exit(1);
    }

    int connection = connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address));
    if (connection < 0)
    {
        perror("\nCHAT CLIENT ERROR: connect() call failed\n");
        close(client_fd);
        exit(1);
    }

    ssize_t bytes_sent;
    ssize_t bytes_read;
    int keep_running_chat = 1;
    while (keep_running_chat)
    {
        bytes_read = recv(client_fd, client_chat_buffer, buffer_size - 1, 0);
        if (bytes_read < 1)
        {
            printf("\nMessage read error.\t");
            close(client_fd);
            break;
        }
        client_chat_buffer[bytes_read] = '\0';

        printf("Host: %s", client_chat_buffer);
        printf("You: ");

        if (fgets(client_chat_buffer, buffer_size, stdin) == NULL)
        {
            perror("\nCHAT CLIENT ERROR: message enter error\n");
            close(client_fd);
            exit(1);
        }

        bytes_sent = send(client_fd, client_chat_buffer, strlen(client_chat_buffer), 0);
        if (bytes_sent < 0)
        {
            printf("\nMessage send error. Try again: \t");
            close(client_fd);
            continue;
        }
    }

    close(client_fd);
}

int main()
{
    int user_choice = 0;

    printf("Choice option:\n1) Host chat\n2) Connect to chat\nYour choice: ");
    scanf("%d", &user_choice);
    getchar();

    switch (user_choice)
    {
    case 1:
        printf("\n");
        start_server(SERVER_PORT);
        break;
    case 2:
        printf("\n");
        char ip[] = "127.0.0.1";
        start_client(ip, SERVER_PORT);
        break;

    default:
        printf("\nIncorrect choice\n");
        break;
    }

    return 0;
}