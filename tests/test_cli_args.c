#include <assert.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "cli_args.h"
#include "test_support.h"

static void test_parse_host_mode(void)
{
    char *argv[] = {"chat", "-m", "host"};
    assert(parse_valid_mode(3, argv) == MODE_HOST);
}

static void test_parse_client_mode_with_other_arguments(void)
{
    char *argv[] = {"chat", "-ip", "127.0.0.1", "-m", "client"};
    assert(parse_valid_mode(5, argv) == MODE_CLIENT);
}

static void test_reject_invalid_or_missing_mode(void)
{
    char *missing[] = {"chat"};
    char *invalid[] = {"chat", "-m", "server"};
    char *missing_value[] = {"chat", "-m"};

    assert(parse_valid_mode(1, missing) == 0);
    assert(parse_valid_mode(3, invalid) == 0);
    assert(parse_valid_mode(2, missing_value) == 0);
}

static void test_parse_ipv4_address(void)
{
    struct in_addr actual;
    struct in_addr expected;
    char *argv[] = {"chat", "-ip", "192.0.2.15"};

    assert(inet_pton(AF_INET, "192.0.2.15", &expected) == 1);
    assert(parse_valid_ip4(3, argv, &actual) == 1);
    assert(actual.s_addr == expected.s_addr);
}

static void test_reject_invalid_or_missing_ipv4_address(void)
{
    struct in_addr actual;
    char *invalid[] = {"chat", "-ip", "999.0.2.15"};
    char *missing[] = {"chat"};
    char *missing_value[] = {"chat", "-ip"};

    assert(parse_valid_ip4(3, invalid, &actual) == 0);
    assert(parse_valid_ip4(1, missing, &actual) == 0);
    assert(parse_valid_ip4(2, missing_value, &actual) == 0);
}

static void test_parse_port_boundaries_and_default(void)
{
    char *minimum[] = {"chat", "-p", "1"};
    char *maximum[] = {"chat", "-p", "65535"};
    char *absent[] = {"chat"};

    assert(parse_valid_port(3, minimum) == 1);
    assert(parse_valid_port(3, maximum) == 65535);
    assert(parse_valid_port(1, absent) == 0);
}

static void test_reject_invalid_port_values(void)
{
    char *zero[] = {"chat", "-p", "0"};
    char *too_large[] = {"chat", "-p", "65536"};
    char *negative[] = {"chat", "-p", "-1"};
    char *non_numeric[] = {"chat", "-p", "abc"};
    char *trailing_characters[] = {"chat", "-p", "80x"};
    char *missing_value[] = {"chat", "-p"};

    assert(parse_valid_port(3, zero) == 0);
    assert(parse_valid_port(3, too_large) == 0);
    assert(parse_valid_port(3, negative) == 0);
    assert(parse_valid_port(3, non_numeric) == 0);
    assert(parse_valid_port(3, trailing_characters) == 0);
    assert(parse_valid_port(2, missing_value) == 0);
}

int main(void)
{
    unsigned passed = 0;
    unsigned total = 0;

    RUN_TEST(test_parse_host_mode, passed, total);
    RUN_TEST(test_parse_client_mode_with_other_arguments, passed, total);
    RUN_TEST(test_reject_invalid_or_missing_mode, passed, total);
    RUN_TEST(test_parse_ipv4_address, passed, total);
    RUN_TEST(test_reject_invalid_or_missing_ipv4_address, passed, total);
    RUN_TEST(test_parse_port_boundaries_and_default, passed, total);
    RUN_TEST(test_reject_invalid_port_values, passed, total);

    puts("cli_args.c unit tests passed");
    PRINT_TEST_RESULT(passed, total);
    return 0;
}
