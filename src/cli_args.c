#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define DEFAULT_PORT 8082

int parse_valid_mode(int argc, char *argv[])
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

int parse_valid_ip4(int argc, char *argv[], struct in_addr *buffer)
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

int parse_valid_port(int argc, char *argv[])
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
