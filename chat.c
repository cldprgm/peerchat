#include "chat.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

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
    while (sent_bytes < (ssize_t)buf_size)
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

static void restore_terminal(Args *args)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &args->old_terminal);
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

void run_chat(int file_descriptor)
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
