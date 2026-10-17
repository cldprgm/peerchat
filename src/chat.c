#include "chat.h"
#include "crypto.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

static int recv_all(int fd, char *buf, size_t buf_size, int flags)
{
    ssize_t recv_bytes = 0;
    while (recv_bytes < (ssize_t)buf_size)
    {
        ssize_t n = recv(fd, buf + recv_bytes, (ssize_t)buf_size - recv_bytes, flags);

        if (n > 0)
        {
            recv_bytes += n;
            continue;
        }

        if (n == 0)
        {
            if (recv_bytes == 0)
                return 0;

            errno = ECONNRESET;
            return -1;
        }

        if (errno == EINTR)
            continue;

        return -1;
    }
    return 1;
}

static int send_all(int fd, const char *buf, size_t buf_size, int flags)
{
    ssize_t sent_bytes = 0;
    while (sent_bytes < (ssize_t)buf_size)
    {
        ssize_t n = send(fd, buf + sent_bytes, (ssize_t)buf_size - sent_bytes, flags);

        if (n > 0)
        {
            sent_bytes += n;
            continue;
        }

        if (n < 0 && errno == EINTR)
            continue;

        if (n == 0)
            errno = EPIPE;

        return -1;
    }
    return 0;
}

static void restore_terminal(void *arg)
{
    Args *args = arg;
    tcsetattr(STDIN_FILENO, TCSANOW, &args->old_terminal);
}

static void *recv_thread(void *arg)
{
    Args *args = arg;
    char buffer[BUFFER_SIZE];
    int res;

    unsigned char ciphertext[BUFFER_SIZE];

    while (1)
    {
        // recv message len
        uint32_t net_len = 0;
        res = recv_all(args->file_descriptor, (char *)&net_len, sizeof(net_len), 0);

        if (res == 0)
        {
            pthread_mutex_lock(&args->mutex);

            args->running = 0;
            printf("\r\033[2K");
            printf("\nConnection closed.\n");
            fflush(stdout);

            pthread_mutex_unlock(&args->mutex);
            break;
        }
        if (res < 0)
        {
            pthread_mutex_lock(&args->mutex);

            args->running = 0;
            printf("\r\033[2K");
            perror("recv message len");

            pthread_mutex_unlock(&args->mutex);
            break;
        }

        net_len = ntohl(net_len);
        if (net_len > BUFFER_SIZE || net_len < crypto_secretstream_xchacha20poly1305_ABYTES)
        {
            pthread_mutex_lock(&args->mutex);

            args->running = 0;
            perror("invalid encrypted message length");

            pthread_mutex_unlock(&args->mutex);
            break;
        }

        // recv encrypt message
        res = recv_all(args->file_descriptor, (char *)ciphertext, (size_t)net_len, 0);
        if (res == 0)
        {
            pthread_mutex_lock(&args->mutex);

            args->running = 0;
            printf("\r\033[2K");
            printf("\nConnection closed before ciphertext was received.\n");
            fflush(stdout);

            pthread_mutex_unlock(&args->mutex);
            break;
        }
        if (res < 0)
        {
            pthread_mutex_lock(&args->mutex);

            args->running = 0;
            printf("\r\033[2K");
            perror("recv encrypted message");

            pthread_mutex_unlock(&args->mutex);
            break;
        }

        // decrypt message
        unsigned long long buffer_len = 0;

        if (decrypt_message(
                args->session,
                ciphertext,
                (unsigned long long)net_len,
                (unsigned char *)buffer,
                &buffer_len) != 0)
        {
            fprintf(stderr, "\ndecrypt_message failed. ciphertext length = %u\n", (unsigned int)net_len);

            args->running = 0;
            break;
        }

        // print message
        if (buffer_len >= sizeof(buffer))
        {
            fprintf(stderr, "decrypted message too large\n");

            args->running = 0;
            break;
        }

        buffer[buffer_len] = '\0';

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

    unsigned char ciphertext[BUFFER_SIZE];
    unsigned char plaintext[BUFFER_SIZE];
    int res;

    // terminal conf
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

    pthread_cleanup_push(restore_terminal, args);

    pthread_mutex_lock(&args->mutex);

    printf("You: ");
    fflush(stdout);

    pthread_mutex_unlock(&args->mutex);

    while (args->running)
    {
        // read char
        char ch;
        ssize_t bytes_read = read(STDIN_FILENO, &ch, 1);
        if (bytes_read < 1)
        {
            args->running = 0;
            shutdown(args->file_descriptor, SHUT_RDWR);
            break;
        }

        size_t message_len = 0;
        int should_send = 0;

        pthread_mutex_lock(&args->mutex);

        // ch - Enter
        if (ch == '\n' || ch == '\r')
        {
            putchar('\n');

            if (args->input_buffer_size > 0)
            {
                message_len = args->input_buffer_size;

                memcpy(plaintext, args->input_buffer, message_len);

                // clear buffer
                args->input_buffer_size = 0;
                args->input_buffer[0] = '\0';

                should_send = 1;
            }
            printf("You: ");
            fflush(stdout);
        }
        // ch - Backspace
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
        // ch - any symbol
        else if ((unsigned char)ch > 31)
        {
            if (args->input_buffer_size < BUFFER_SIZE - crypto_secretstream_xchacha20poly1305_ABYTES)
            {
                args->input_buffer[args->input_buffer_size++] = ch;
                args->input_buffer[args->input_buffer_size] = '\0';

                putchar(ch);
                fflush(stdout);
            }
        }

        // unlock for network operations
        pthread_mutex_unlock(&args->mutex);

        if (!should_send)
            continue;

        // encrypt message
        unsigned long long ciphertext_len = 0;

        if (encrypt_message(
                args->session,
                plaintext,
                (unsigned long long)message_len,
                ciphertext,
                &ciphertext_len) != 0)
        {
            perror("encrypt message");
            args->running = 0;
            shutdown(args->file_descriptor, SHUT_RDWR);
            break;
        }

        // send message len
        uint32_t net_len = htonl((uint32_t)ciphertext_len);
        res = send_all(args->file_descriptor, (const char *)&net_len, sizeof(net_len), 0);
        if (res != 0)
        {
            perror("send message len");
            args->running = 0;
            shutdown(args->file_descriptor, SHUT_RDWR);
            break;
        }

        // send encrypt message
        res = send_all(args->file_descriptor, (const char *)ciphertext, (size_t)ciphertext_len, 0);
        if (res != 0)
        {
            perror("send encrypt message");
            args->running = 0;
            shutdown(args->file_descriptor, SHUT_RDWR);
            break;
        }
    }

    pthread_cleanup_pop(1);

    return NULL;
}

void run_chat(int file_descriptor, crypto_session *session)
{
    Args args = {.input_buffer = {0}, .input_buffer_size = 0, .session = session, .file_descriptor = file_descriptor, .running = 1};

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
