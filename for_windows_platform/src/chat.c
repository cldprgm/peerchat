#include "chat.h"
#include "crypto.h"

#include <conio.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int recv_all(SOCKET socket_fd, char *buffer, size_t buffer_size, int flags)
{
    size_t received_bytes = 0;
    while (received_bytes < buffer_size)
    {
        int remaining = (int)(buffer_size - received_bytes);
        int received = recv(socket_fd, buffer + received_bytes, remaining, flags);

        if (received > 0)
        {
            received_bytes += (size_t)received;
            continue;
        }

        if (received == 0)
        {
            if (received_bytes == 0)
                return 0;

            WSASetLastError(WSAECONNRESET);
            errno = ECONNRESET;
            return -1;
        }

        if (WSAGetLastError() == WSAEINTR)
            continue;

        return -1;
    }
    return 1;
}

static int send_all(SOCKET socket_fd, const char *buffer, size_t buffer_size, int flags)
{
    size_t sent_bytes = 0;
    while (sent_bytes < buffer_size)
    {
        int remaining = (int)(buffer_size - sent_bytes);
        int sent = send(socket_fd, buffer + sent_bytes, remaining, flags);

        if (sent > 0)
        {
            sent_bytes += (size_t)sent;
            continue;
        }

        if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEINTR)
            continue;

        if (sent == 0)
        {
            WSASetLastError(WSAECONNRESET);
            errno = EPIPE;
        }

        return -1;
    }
    return 0;
}

static void print_socket_error(const char *message)
{
    fprintf(stderr, "%s failed (Winsock error %d)\n", message, WSAGetLastError());
}

static void set_running(Args *args, LONG value)
{
    InterlockedExchange(&args->running, value);
}

static int is_running(Args *args)
{
    return InterlockedCompareExchange(&args->running, 0, 0) != 0;
}

static DWORD WINAPI recv_thread(LPVOID arg)
{
    Args *args = (Args *)arg;
    char buffer[BUFFER_SIZE];
    unsigned char ciphertext[BUFFER_SIZE];

    while (is_running(args))
    {
        uint32_t net_len = 0;
        int result = recv_all(args->file_descriptor, (char *)&net_len, sizeof(net_len), 0);

        if (result == 0)
        {
            EnterCriticalSection(&args->mutex);
            set_running(args, 0);
            printf("\r\033[2K\nConnection closed.\n");
            fflush(stdout);
            LeaveCriticalSection(&args->mutex);
            break;
        }
        if (result < 0)
        {
            EnterCriticalSection(&args->mutex);
            set_running(args, 0);
            printf("\r\033[2K");
            print_socket_error("recv message len");
            LeaveCriticalSection(&args->mutex);
            break;
        }

        net_len = ntohl(net_len);
        if (net_len > BUFFER_SIZE || net_len < crypto_secretstream_xchacha20poly1305_ABYTES)
        {
            EnterCriticalSection(&args->mutex);
            set_running(args, 0);
            fprintf(stderr, "invalid encrypted message length: %u\n", (unsigned int)net_len);
            LeaveCriticalSection(&args->mutex);
            break;
        }

        result = recv_all(args->file_descriptor, (char *)ciphertext, (size_t)net_len, 0);
        if (result == 0)
        {
            EnterCriticalSection(&args->mutex);
            set_running(args, 0);
            printf("\r\033[2K\nConnection closed before ciphertext was received.\n");
            fflush(stdout);
            LeaveCriticalSection(&args->mutex);
            break;
        }
        if (result < 0)
        {
            EnterCriticalSection(&args->mutex);
            set_running(args, 0);
            printf("\r\033[2K");
            print_socket_error("recv encrypted message");
            LeaveCriticalSection(&args->mutex);
            break;
        }

        unsigned long long buffer_len = 0;
        if (decrypt_message(args->session, ciphertext, (unsigned long long)net_len,
                            (unsigned char *)buffer, &buffer_len) != 0)
        {
            fprintf(stderr, "\ndecrypt_message failed. ciphertext length = %u\n", (unsigned int)net_len);
            set_running(args, 0);
            break;
        }

        if (buffer_len >= sizeof(buffer))
        {
            fprintf(stderr, "decrypted message too large\n");
            set_running(args, 0);
            break;
        }
        buffer[buffer_len] = '\0';

        EnterCriticalSection(&args->mutex);
        printf("\r\033[2K");
        printf("User: %s\n", buffer);
        printf("You: ");
        fwrite(args->input_buffer, 1, args->input_buffer_size, stdout);
        fflush(stdout);
        LeaveCriticalSection(&args->mutex);
    }

    return 0;
}

static DWORD WINAPI send_thread(LPVOID arg)
{
    Args *args = (Args *)arg;
    unsigned char ciphertext[BUFFER_SIZE];
    unsigned char plaintext[BUFFER_SIZE];

    EnterCriticalSection(&args->mutex);
    printf("You: ");
    fflush(stdout);
    LeaveCriticalSection(&args->mutex);

    while (is_running(args))
    {
        if (!_kbhit())
        {
            Sleep(20);
            continue;
        }

        int key = _getch();
        if (key == 0 || key == 0xE0)
        {
            (void)_getch();
            continue;
        }
        char ch = (char)key;
        size_t message_len = 0;
        int should_send = 0;

        EnterCriticalSection(&args->mutex);
        if (ch == '\n' || ch == '\r')
        {
            putchar('\n');
            if (args->input_buffer_size > 0)
            {
                message_len = args->input_buffer_size;
                memcpy(plaintext, args->input_buffer, message_len);
                args->input_buffer_size = 0;
                args->input_buffer[0] = '\0';
                should_send = 1;
            }
            printf("You: ");
            fflush(stdout);
        }
        else if (ch == '\b')
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
            if (args->input_buffer_size < BUFFER_SIZE - crypto_secretstream_xchacha20poly1305_ABYTES)
            {
                args->input_buffer[args->input_buffer_size++] = ch;
                args->input_buffer[args->input_buffer_size] = '\0';
                putchar(ch);
                fflush(stdout);
            }
        }
        LeaveCriticalSection(&args->mutex);

        if (!should_send)
            continue;

        unsigned long long ciphertext_len = 0;
        if (encrypt_message(args->session, plaintext, (unsigned long long)message_len,
                            ciphertext, &ciphertext_len) != 0)
        {
            fprintf(stderr, "encrypt_message failed\n");
            set_running(args, 0);
            shutdown(args->file_descriptor, SD_BOTH);
            break;
        }

        uint32_t net_len = htonl((uint32_t)ciphertext_len);
        if (send_all(args->file_descriptor, (const char *)&net_len, sizeof(net_len), 0) != 0)
        {
            print_socket_error("send message len");
            set_running(args, 0);
            shutdown(args->file_descriptor, SD_BOTH);
            break;
        }

        if (send_all(args->file_descriptor, (const char *)ciphertext, (size_t)ciphertext_len, 0) != 0)
        {
            print_socket_error("send encrypted message");
            set_running(args, 0);
            shutdown(args->file_descriptor, SD_BOTH);
            break;
        }
    }

    return 0;
}

void run_chat(SOCKET socket_fd, crypto_session *session)
{
    Args args = {0};
    args.session = session;
    args.file_descriptor = socket_fd;
    args.running = 1;
    InitializeCriticalSection(&args.mutex);

    HANDLE recv_thread_handle = CreateThread(NULL, 0, recv_thread, &args, 0, NULL);
    if (recv_thread_handle == NULL)
    {
        fprintf(stderr, "CreateThread(recv_thread) failed (Windows error %lu)\n", GetLastError());
        DeleteCriticalSection(&args.mutex);
        return;
    }

    HANDLE send_thread_handle = CreateThread(NULL, 0, send_thread, &args, 0, NULL);
    if (send_thread_handle == NULL)
    {
        fprintf(stderr, "CreateThread(send_thread) failed (Windows error %lu)\n", GetLastError());
        set_running(&args, 0);
        shutdown(socket_fd, SD_BOTH);
        WaitForSingleObject(recv_thread_handle, INFINITE);
        CloseHandle(recv_thread_handle);
        DeleteCriticalSection(&args.mutex);
        return;
    }

    HANDLE threads[] = {recv_thread_handle, send_thread_handle};
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    CloseHandle(recv_thread_handle);
    CloseHandle(send_thread_handle);
    DeleteCriticalSection(&args.mutex);
}
