#ifndef NETWORK_H
#define NETWORK_H

#include <netinet/in.h>

void start_server(int port);

void start_client(const struct in_addr *addr, int port);

#endif