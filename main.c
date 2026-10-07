#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define DEFAULT_PORT 8082
#define BUFFER_SIZE 2048
#define BACKLOG 1

typedef struct
{
    char input_buffer[BUFFER_SIZE];
    size_t input_buffer_size;

    pthread_mutex_t mutex;

    struct termios old_terminal;

    int file_descriptor;
    int running;
} Args;

static void restore_terminal(Args *args)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &args->old_terminal);
}

static ssize_t recv_message(int fd, char *buf, size_t buf_size, int flags)
{
    static char recv_buffer[BUFFER_SIZE];
    static size_t recv_size = 0;

    while (1)
    {
        for (size_t i = 0; i < recv_size; i++)
        {
            if (recv_buffer[i] == '\n')
            {
                size_t message_size = i;

                if (message_size >= buf_size)
                    return -2;

                memcpy(buf, recv_buffer, message_size);
                buf[message_size] = '\0';

                size_t remaining = recv_size - (message_size + 1);

                memmove(recv_buffer, recv_buffer + message_size + 1, remaining);

                recv_size = remaining;

                return (ssize_t)message_size;
            }
        }

        if (recv_size == sizeof(recv_buffer))
            return -2;

        ssize_t n = recv(fd, recv_buffer + recv_size, sizeof(recv_buffer) - recv_size, flags);
        if (n == 0)
            return 0;

        if (n < 0)
        {
            if (errno == EINTR)
                continue;

            return -1;
        }

        recv_size += (size_t)n;
    }
}

static ssize_t send_message(int fd, const char *buf, size_t buf_size, int flags)
{
    ssize_t sent_bytes = 0;
    while (sent_bytes < buf_size)
    {
        ssize_t n = send(fd, buf + sent_bytes, (ssize_t)buf_size - sent_bytes, flags);
        if (n == 0)
            return 0;
        if (n < 0)
            return -1;

        sent_bytes += n;
    }
    return sent_bytes;
}

static void *recv_thread(void *arg)
{
    Args *args = arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while (1)
    {
        bytes_read = recv_message(args->file_descriptor, buffer, sizeof(buffer), 0);
        if (bytes_read == 0)
        {
            pthread_mutex_lock(&args->mutex);

            args->running = 0;
            printf("\r\033[2K");
            printf("\nConnection closed.\n");
            fflush(stdout);

            pthread_mutex_unlock(&args->mutex);
            break;
        }
        if (bytes_read < 0)
        {
            pthread_mutex_lock(&args->mutex);

            args->running = 0;
            printf("\r\033[2K");
            perror("recv");

            pthread_mutex_unlock(&args->mutex);
            break;
        }

        pthread_mutex_lock(&args->mutex);

        printf("\r\033[2K");
        printf("User: %s\n", buffer);

        printf("You: ");
        fwrite(args->input_buffer, 1, args->input_buffer_size, stdout);
        fflush(stdout);

        pthread_mutex_unlock(&args->mutex);
    }

    return NULL;
}

static void *send_thread(void *arg)
{
    Args *args = arg;

    struct termios raw_terminal;
    if (tcgetattr(STDIN_FILENO, &args->old_terminal) < 0)
    {
        perror("tcgetattr");
        return NULL;
    }
    raw_terminal = args->old_terminal;
    raw_terminal.c_lflag &= ~(ICANON | ECHO);
    raw_terminal.c_cc[VMIN] = 1;
    raw_terminal.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw_terminal) < 0)
    {
        perror("tcsetattr");
        return NULL;
    }

    pthread_cleanup_push((void (*)(void *))restore_terminal, args);

    pthread_mutex_lock(&args->mutex);

    printf("You: ");
    fflush(stdout);

    pthread_mutex_unlock(&args->mutex);

    while (args->running)
    {
        char ch;
        ssize_t bytes_read = read(STDIN_FILENO, &ch, 1);
        if (bytes_read < 1)
            break;

        pthread_mutex_lock(&args->mutex);

        if (ch == '\n' || ch == '\r')
        {
            if (args->input_buffer_size > 0)
            {
                putchar('\n');

                args->input_buffer[args->input_buffer_size] = '\n';

                ssize_t sent = send_message(args->file_descriptor, args->input_buffer, args->input_buffer_size + 1, 0);
                if (sent < 0)
                {
                    perror("send");
                    args->running = 0;
                }

                args->input_buffer_size = 0;
                args->input_buffer[0] = '\0';

                if (args->running)
                {
                    printf("You: ");
                    fflush(stdout);
                }
            }
            else
            {
                putchar('\n');
                printf("You: ");
                fflush(stdout);
            }
        }
        else if (ch == 127 || ch == '\b')
        {
            if (args->input_buffer_size > 0)
            {
                args->input_buffer_size--;
                args->input_buffer[args->input_buffer_size] = '\0';

                printf("\b \b");
                fflush(stdout);
            }
        }
        else if ((unsigned char)ch > 31)
        {
            if (args->input_buffer_size < BUFFER_SIZE - 2)
            {
                args->input_buffer[args->input_buffer_size++] = ch;
                args->input_buffer[args->input_buffer_size] = '\0';

                putchar(ch);
                fflush(stdout);
            }
        }

        pthread_mutex_unlock(&args->mutex);
    }

    pthread_cleanup_pop(1);

    return NULL;
}

static void die(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

static void run_chat(int file_descriptor)
{
    Args args = {.input_buffer = {0}, .input_buffer_size = 0, .file_descriptor = file_descriptor, .running = 1};

    pthread_mutex_init(&args.mutex, NULL);

    pthread_t recv_thread_id;
    pthread_t send_thread_id;

    if (pthread_create(&recv_thread_id, NULL, recv_thread, &args) != 0)
    {
        fprintf(stderr, "pthread_create(recv_thread) failed\n");

        pthread_mutex_destroy(&args.mutex);
        return;
    }

    if (pthread_create(&send_thread_id, NULL, send_thread, &args) != 0)
    {
        fprintf(stderr, "pthread_create(send_thread) failed\n");

        args.running = 0;
        pthread_cancel(recv_thread_id);
        pthread_join(recv_thread_id, NULL);

        pthread_mutex_destroy(&args.mutex);
        return;
    }

    pthread_join(recv_thread_id, NULL);
    pthread_cancel(send_thread_id);
    pthread_join(send_thread_id, NULL);
    pthread_mutex_destroy(&args.mutex);
}

static void start_server(int port)
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

static void start_client(const struct in_addr *addr, int port)
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

static int parse_valid_mode(int argc, char *argv[])
{
    int res = 0;

    for (size_t i = 1; i < (size_t)argc; i++)
    {
        if (strcmp(argv[i], "-m") == 0)
        {
            if (i + 1 >= (size_t)argc)
            {
                fprintf(stderr, "'-m' requires a value\n");
                return 0;
            }

            if (strcmp(argv[i + 1], "host") == 0)
                res = 1;
            else if (strcmp(argv[i + 1], "client") == 0)
                res = 2;
            else
            {
                fprintf(stderr, "Invalid value for '-m': %s. Expected 'host' or 'client'\n", argv[i + 1]);
                return 0;
            }

            break;
        }
    }
    if (res == 0)
    {
        fprintf(stderr, "Required argument '-m' is missing\n");
        return 0;
    }

    return res;
}

static int parse_valid_ip4(int argc, char *argv[], struct in_addr *buffer)
{
    for (size_t i = 1; i < (size_t)argc; i++)
    {
        if (strcmp(argv[i], "-ip") == 0)
        {
            if (i + 1 >= (size_t)argc)
            {
                fprintf(stderr, "'-ip' requires a value\n");
                return 0;
            }

            if (inet_pton(AF_INET, argv[i + 1], buffer) == 1)
            {
                return 1;
            }
            fprintf(stderr, "Invalid IPv4 address: %s\n", argv[i + 1]);
            return 0;
        }
    }

    fprintf(stderr, "Required argument '-ip' is missing\n");
    return 0;
}

static int parse_valid_port(int argc, char *argv[])
{
    int res = 0;

    for (size_t i = 1; i < (size_t)argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            if (i + 1 >= (size_t)argc)
            {
                fprintf(stderr, "'-p' requires a value (using default port %d)\n", DEFAULT_PORT);
                return 0;
            }
            errno = 0;
            char *end;
            long port = strtol(argv[i + 1], &end, 10);

            if (errno != 0 || end == argv[i + 1] || *end != '\0' || port < 1 || port > 65535)
            {
                fprintf(stderr, "'-p' value is invalid (use: 1-65535, using default port %d)\n", DEFAULT_PORT);
                return 0;
            }

            res = (int)port;
            break;
        }
    }

    return res;
}

int main(int argc, char *argv[])
{
    int user_mode_choice;
    if ((user_mode_choice = parse_valid_mode(argc, argv)) == 0)
        return 1;

    struct in_addr user_addr_choice;
    if (user_mode_choice == 2)
    {
        if (parse_valid_ip4(argc, argv, &user_addr_choice) == 0)
            return 1;
    }

    int user_port_choice;
    if ((user_port_choice = parse_valid_port(argc, argv)) == 0)
        user_port_choice = DEFAULT_PORT;

    switch (user_mode_choice)
    {
    case 1:
        printf("\n");
        start_server(user_port_choice);
        break;
    case 2:
        printf("\n");
        start_client(&user_addr_choice, user_port_choice);
        break;

    default:
        printf("\nIncorrect mode (use: 'host' or 'client')\n");
        break;
    }

    return 0;
}