#ifndef NETWORK_H
#define NETWORK_H

#include <netinet/in.h>
#include <sodium.h>

typedef struct
{
    unsigned char pk[crypto_kx_PUBLICKEYBYTES];
    unsigned char sk[crypto_kx_SECRETKEYBYTES];

    unsigned char rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char tx[crypto_kx_SESSIONKEYBYTES];

    unsigned char peer_pk[crypto_kx_PUBLICKEYBYTES];
} crypto_kx_keys;

void start_server(int port);

void start_client(const struct in_addr *addr, int port);

#endif