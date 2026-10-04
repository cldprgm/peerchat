#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 8082

typedef struct
{
    char *buffer;
    size_t buffer_size;
    int file_descriptor;
} Args;

void *recv_thread(void *arg)
{
    Args *args = (Args *)arg;
    ssize_t bytes_read;

    while (1)
    {
        bytes_read = recv(args->file_descriptor, args->buffer, args->buffer_size, 0);
        if (bytes_read < 1)
        {
            printf("\nMessage read error.\t");
            break;
        }
        args->buffer[bytes_read] = '\0';

        printf("User: %s", args->buffer);
    }

    return NULL;
}

void *send_thread(void *arg)
{
    Args *args = (Args *)arg;
    size_t len;
    ssize_t bytes_sent;

    while (1)
    {
        printf("You: ");

        if (fgets(args->buffer, args->buffer_size, stdin) == NULL)
        {
            perror("\nCHAT CLIENT ERROR: message enter error\n");
            break;
        }

        len = strlen(args->buffer);
        bytes_sent = send(args->file_descriptor, args->buffer, len, 0);
        if (bytes_sent < 0)
        {
            printf("\nMessage send error. Try again: \t");
            break;
        }
    }

    return NULL;
}

void start_server(int port)
{
    char recv_buffer[2048];
    char send_buffer[2048];
    size_t recv_buffer_size = sizeof(recv_buffer) - 1;
    size_t send_buffer_size = sizeof(send_buffer) - 1;

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

    Args recv_args = {.buffer = recv_buffer, .buffer_size = recv_buffer_size, .file_descriptor = client_fd};
    Args send_args = {.buffer = send_buffer, .buffer_size = send_buffer_size, .file_descriptor = client_fd};

    pthread_t recv_thread_id;
    pthread_t send_thread_id;

    pthread_create(&recv_thread_id, NULL, recv_thread, &recv_args);
    pthread_create(&send_thread_id, NULL, send_thread, &send_args);

    pthread_join(recv_thread_id, NULL);
    pthread_join(send_thread_id, NULL);

    close(client_fd);
    close(server_fd);
}

void start_client(const char *ip_addr, int port)
{
    char recv_buffer[2048];
    char send_buffer[2048];
    size_t recv_buffer_size = sizeof(recv_buffer) - 1;
    size_t send_buffer_size = sizeof(send_buffer) - 1;

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

    printf("Successfully connected!\n\n");

    Args recv_args = {.buffer = recv_buffer, .buffer_size = recv_buffer_size, .file_descriptor = client_fd};
    Args send_args = {.buffer = send_buffer, .buffer_size = send_buffer_size, .file_descriptor = client_fd};

    pthread_t recv_thread_id;
    pthread_t send_thread_id;

    pthread_create(&recv_thread_id, NULL, recv_thread, &recv_args);
    pthread_create(&send_thread_id, NULL, send_thread, &send_args);

    pthread_join(recv_thread_id, NULL);
    pthread_join(send_thread_id, NULL);

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