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
#define BUFFER_SIZE 2048
#define BACKLOG 3

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

static void die(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

static void run_chat(int file_descriptor, const int buffer_size)
{
    char recv_buffer[buffer_size];
    char send_buffer[buffer_size];

    Args recv_args = {.buffer = recv_buffer, .buffer_size = sizeof(recv_buffer) - 1, .file_descriptor = file_descriptor};
    Args send_args = {.buffer = send_buffer, .buffer_size = sizeof(send_buffer) - 1, .file_descriptor = file_descriptor};

    pthread_t recv_thread_id;
    pthread_t send_thread_id;

    if (pthread_create(&recv_thread_id, NULL, recv_thread, &recv_args) != 0)
    {
        fprintf(stderr, "pthread_create(recv_thread) failed\n");
        return;
    }

    if (pthread_create(&send_thread_id, NULL, send_thread, &send_args) != 0)
    {
        fprintf(stderr, "pthread_create(send_thread) failed\n");
        pthread_cancel(recv_thread_id);
        pthread_join(recv_thread_id, NULL);
        return;
    }

    pthread_join(recv_thread_id, NULL);
    pthread_join(send_thread_id, NULL);
}

void start_server(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        die("socket");

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        die("setsockopt");

    struct sockaddr_in client_address;
    struct sockaddr_in server_address;

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

    printf("Waiting for client...\n");

    socklen_t client_address_len = sizeof(client_address);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len);
    if (client_fd < 0)
    {
        close(server_fd);
        die("accept");
    }

    printf("Client connected!\n\n");

    run_chat(client_fd, BUFFER_SIZE);

    close(client_fd);
    close(server_fd);
}

void start_client(const char *ip_addr, int port)
{
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
        die("socket");

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_addr, &server_address.sin_addr) != 1)
    {
        close(client_fd);
        fprintf(stderr, "Invalid IP address: %s\n", ip_addr);
        exit(EXIT_FAILURE);
    }

    if (connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        close(client_fd);
        die("connect");
    }

    printf("Successfully connected!\n\n");

    run_chat(client_fd, BUFFER_SIZE);

    close(client_fd);
}

int main()
{
    int user_choice = 0;

    printf("Choice option:\n1) Host chat\n2) Connect to chat\n\nYour choice: ");
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