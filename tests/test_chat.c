#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "test_support.h"

#define static
#include "../src/chat.c"
#undef static

static void test_recv_all_reads_full_buffer(void)
{
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    const char expected[] = "fragment-safe socket read";
    char actual[sizeof(expected)] = {0};
    assert(send_all(fds[1], expected, sizeof(expected), 0) == 0);
    assert(recv_all(fds[0], actual, sizeof(actual), 0) == 1);
    assert(memcmp(actual, expected, sizeof(expected)) == 0);

    close(fds[0]);
    close(fds[1]);
}

static void test_recv_all_distinguishes_eof_and_partial_message(void)
{
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    char buffer[8];
    close(fds[1]);
    assert(recv_all(fds[0], buffer, sizeof(buffer), 0) == 0);
    close(fds[0]);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    assert(send(fds[1], "abc", 3, 0) == 3);
    close(fds[1]);
    errno = 0;
    assert(recv_all(fds[0], buffer, sizeof(buffer), 0) == -1);
    assert(errno == ECONNRESET);
    close(fds[0]);
}

static void test_send_all_writes_full_buffer_and_rejects_bad_fd(void)
{
    int fds[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    const char expected[] = "complete socket write";
    char actual[sizeof(expected)] = {0};
    assert(send_all(fds[0], expected, sizeof(expected), 0) == 0);
    assert(recv_all(fds[1], actual, sizeof(actual), 0) == 1);
    assert(memcmp(actual, expected, sizeof(expected)) == 0);
    assert(send_all(-1, expected, sizeof(expected), 0) == -1);

    close(fds[0]);
    close(fds[1]);
}

int main(void)
{
    unsigned passed = 0;
    unsigned total = 0;

    RUN_TEST(test_recv_all_reads_full_buffer, passed, total);
    RUN_TEST(test_recv_all_distinguishes_eof_and_partial_message, passed, total);
    RUN_TEST(test_send_all_writes_full_buffer_and_rejects_bad_fd, passed, total);

    puts("chat.c unit tests passed");
    PRINT_TEST_RESULT(passed, total);
    return 0;
}
