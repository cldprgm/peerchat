#ifndef ARGS_H
#define ARGS_H

#include <netinet/in.h>

typedef enum
{
    MODE_HOST = 1,
    MODE_CLIENT
} Mode;

int parse_valid_mode(int argc, char *argv[]);

int parse_valid_ip4(int argc, char *argv[], struct in_addr *buffer);

int parse_valid_port(int argc, char *argv[]);

#endif