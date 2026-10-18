#include <assert.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "network.h"
#include "test_support.h"

static int server_call_count;
static int client_call_count;
static int last_port;
static struct in_addr last_address;

void start_server(int port)
{
    ++server_call_count;
    last_port = port;
}

void start_client(const struct in_addr *addr, int port)
{
    ++client_call_count;
    last_port = port;
    last_address = *addr;
}

#define main application_main
#include "../src/main.c"
#undef main

static void test_host_dispatch(void)
{
    char *argv[] = {"chat", "-m", "host", "-p", "9000"};
    server_call_count = 0;
    client_call_count = 0;

    assert(application_main(5, argv) == 0);
    assert(server_call_count == 1);
    assert(client_call_count == 0);
    assert(last_port == 9000);
}

static void test_client_dispatch(void)
{
    char *argv[] = {"chat", "-m", "client", "-ip", "127.0.0.1", "-p", "9001"};
    struct in_addr expected;
    client_call_count = 0;
    server_call_count = 0;

    assert(inet_pton(AF_INET, "127.0.0.1", &expected) == 1);
    assert(application_main(7, argv) == 0);
    assert(client_call_count == 1);
    assert(server_call_count == 0);
    assert(last_port == 9001);
    assert(last_address.s_addr == expected.s_addr);
}

static void test_host_uses_default_port(void)
{
    char *argv[] = {"chat", "-m", "host"};
    server_call_count = 0;

    assert(application_main(3, argv) == 0);
    assert(server_call_count == 1);
    assert(last_port == 8082);
}

static void test_invalid_arguments_do_not_start_networking(void)
{
    char *invalid_mode[] = {"chat", "-m", "invalid"};
    char *missing_client_ip[] = {"chat", "-m", "client"};
    server_call_count = 0;
    client_call_count = 0;

    assert(application_main(3, invalid_mode) == 1);
    assert(application_main(3, missing_client_ip) == 1);
    assert(server_call_count == 0);
    assert(client_call_count == 0);
}

int main(void)
{
    unsigned passed = 0;
    unsigned total = 0;

    RUN_TEST(test_host_dispatch, passed, total);
    RUN_TEST(test_client_dispatch, passed, total);
    RUN_TEST(test_host_uses_default_port, passed, total);
    RUN_TEST(test_invalid_arguments_do_not_start_networking, passed, total);

    puts("main.c unit tests passed");
    PRINT_TEST_RESULT(passed, total);
    return 0;
}
