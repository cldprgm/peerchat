#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "crypto.h"
#include "test_support.h"

void run_chat(int file_descriptor, crypto_session *session);

#define static
#include "../src/network.c"
#undef static

void run_chat(int file_descriptor, crypto_session *session)
{
    (void)file_descriptor;
    (void)session;
}

static void test_recv_all_reads_full_buffer(void)
{
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    unsigned char expected[128];
    for (size_t i = 0; i < sizeof(expected); ++i)
        expected[i] = (unsigned char)(i * 7U + 3U);

    assert(send_all(fds[1], expected, sizeof(expected), 0) == 0);

    unsigned char actual[sizeof(expected)];
    memset(actual, 0, sizeof(actual));
    assert(recv_all(fds[0], actual, sizeof(actual), 0) == 0);
    assert(memcmp(actual, expected, sizeof(expected)) == 0);

    close(fds[0]);
    close(fds[1]);
}

static void test_recv_all_returns_error_when_peer_closes(void)
{
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    close(fds[1]);

    unsigned char buffer[32] = {0};
    assert(recv_all(fds[0], buffer, sizeof(buffer), 0) == -1);

    close(fds[0]);
}

static void test_send_all_writes_full_buffer(void)
{
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    unsigned char payload[96];
    for (size_t i = 0; i < sizeof(payload); ++i)
        payload[i] = (unsigned char)(i * 13U + 11U);

    assert(send_all(fds[0], payload, sizeof(payload), 0) == 0);

    unsigned char received[sizeof(payload)];
    memset(received, 0, sizeof(received));
    ssize_t total = 0;
    while (total < (ssize_t)sizeof(received))
    {
        ssize_t n = recv(fds[1], received + total, (size_t)sizeof(received) - (size_t)total, 0);
        assert(n > 0);
        total += n;
    }

    assert(memcmp(received, payload, sizeof(payload)) == 0);

    close(fds[0]);
    close(fds[1]);
}

struct key_exchange_ctx
{
    int fd;
    crypto_kx_keys *keys;
    int result;
};

static void *server_exchange_thread(void *arg)
{
    struct key_exchange_ctx *ctx = arg;
    ctx->result = fill_server_keys(ctx->fd, ctx->keys);
    return NULL;
}

static void *client_exchange_thread(void *arg)
{
    struct key_exchange_ctx *ctx = arg;
    ctx->result = fill_client_keys(ctx->fd, ctx->keys);
    return NULL;
}

static void test_fill_server_and_client_keys_exchange(void)
{
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    crypto_kx_keys server_keys = {0};
    crypto_kx_keys client_keys = {0};

    pthread_t server_thread;
    pthread_t client_thread;

    struct key_exchange_ctx server_ctx = { .fd = fds[0], .keys = &server_keys, .result = 0 };
    struct key_exchange_ctx client_ctx = { .fd = fds[1], .keys = &client_keys, .result = 0 };

    assert(pthread_create(&server_thread, NULL, server_exchange_thread, &server_ctx) == 0);
    assert(pthread_create(&client_thread, NULL, client_exchange_thread, &client_ctx) == 0);

    assert(pthread_join(server_thread, NULL) == 0);
    assert(pthread_join(client_thread, NULL) == 0);

    assert(server_ctx.result == 1);
    assert(client_ctx.result == 1);

    assert(memcmp(server_keys.pk, client_keys.peer_pk, sizeof(server_keys.pk)) == 0);
    assert(memcmp(client_keys.pk, server_keys.peer_pk, sizeof(client_keys.pk)) == 0);
    assert(memcmp(server_keys.rx, client_keys.tx, sizeof(server_keys.rx)) == 0);
    assert(memcmp(server_keys.tx, client_keys.rx, sizeof(server_keys.tx)) == 0);

    close(fds[0]);
    close(fds[1]);
}

static void test_fill_server_keys_rejects_null_pointer(void)
{
    crypto_kx_keys keys = {0};
    assert(fill_server_keys(-1, NULL) == -1);
    assert(fill_server_keys(-1, &keys) == -1);
}

static void test_fill_client_keys_rejects_null_pointer(void)
{
    crypto_kx_keys keys = {0};
    assert(fill_client_keys(-1, NULL) == -1);
    assert(fill_client_keys(-1, &keys) == -2);
}

int main(void)
{
    unsigned passed = 0;
    unsigned total = 0;

    assert(sodium_init() >= 0);
    RUN_TEST(test_recv_all_reads_full_buffer, passed, total);
    RUN_TEST(test_recv_all_returns_error_when_peer_closes, passed, total);
    RUN_TEST(test_send_all_writes_full_buffer, passed, total);
    RUN_TEST(test_fill_server_and_client_keys_exchange, passed, total);
    RUN_TEST(test_fill_server_keys_rejects_null_pointer, passed, total);
    RUN_TEST(test_fill_client_keys_rejects_null_pointer, passed, total);

    puts("network.c unit tests passed");
    PRINT_TEST_RESULT(passed, total);
    return 0;
}
