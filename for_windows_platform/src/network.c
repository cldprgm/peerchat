#include "network.h"
#include "crypto.h"
#include "chat.h"

#include <stdio.h>
#include <stdlib.h>
#include <sodium.h>

#define BACKLOG 1

static void die(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void die_socket(const char *message)
{
    fprintf(stderr, "%s failed (Winsock error %d)\n", message, WSAGetLastError());
    exit(EXIT_FAILURE);
}

static int recv_all(SOCKET socket_fd, unsigned char *buffer, size_t buffer_size, int flags)
{
    size_t received_bytes = 0;
    while (received_bytes < buffer_size)
    {
        int remaining = (int)(buffer_size - received_bytes);
        int received = recv(socket_fd, (char *)buffer + received_bytes, remaining, flags);

        if (received == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEINTR)
                continue;
            return -1;
        }
        if (received == 0)
            return -1;

        received_bytes += (size_t)received;
    }
    return 0;
}

static int send_all(SOCKET socket_fd, const unsigned char *buffer, size_t buffer_size, int flags)
{
    size_t sent_bytes = 0;
    while (sent_bytes < buffer_size)
    {
        int remaining = (int)(buffer_size - sent_bytes);
        int sent = send(socket_fd, (const char *)buffer + sent_bytes, remaining, flags);

        if (sent == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEINTR)
                continue;
            return -1;
        }
        if (sent == 0)
            return -1;

        sent_bytes += (size_t)sent;
    }
    return 0;
}

static int fill_server_keys(SOCKET socket_fd, crypto_kx_keys *keys)
{
    if (keys == NULL)
        return -1;

    sodium_memzero(keys, sizeof(*keys));
    crypto_kx_keypair(keys->pk, keys->sk);

    if (send_all(socket_fd, keys->pk, sizeof(keys->pk), 0) < 0)
        return -1;

    if (recv_all(socket_fd, keys->peer_pk, sizeof(keys->peer_pk), 0) < 0)
        return -2;

    if (crypto_kx_server_session_keys(keys->rx, keys->tx,
                                      keys->pk, keys->sk, keys->peer_pk) != 0)
    {
        sodium_memzero(keys->sk, sizeof(keys->sk));
        return -3;
    }

    sodium_memzero(keys->sk, sizeof(keys->sk));
    return 1;
}

static int fill_client_keys(SOCKET socket_fd, crypto_kx_keys *keys)
{
    if (keys == NULL)
        return -1;

    sodium_memzero(keys, sizeof(*keys));
    crypto_kx_keypair(keys->pk, keys->sk);

    if (recv_all(socket_fd, keys->peer_pk, sizeof(keys->peer_pk), 0) < 0)
        return -2;

    if (send_all(socket_fd, keys->pk, sizeof(keys->pk), 0) < 0)
        return -1;

    if (crypto_kx_client_session_keys(keys->rx, keys->tx,
                                      keys->pk, keys->sk, keys->peer_pk) != 0)
    {
        sodium_memzero(keys->sk, sizeof(keys->sk));
        return -3;
    }

    sodium_memzero(keys->sk, sizeof(keys->sk));
    return 1;
}

static void close_socket(SOCKET socket_fd)
{
    if (socket_fd != INVALID_SOCKET)
        closesocket(socket_fd);
}

static const char *key_exchange_error(int result)
{
    if (result == -1)
        return "send_key";
    if (result == -2)
        return "recv_key";
    if (result == -3)
        return "suspicious user public key";
    return "unknown key exchange error";
}

void start_server(int port)
{
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET)
        die_socket("socket");

    BOOL reuse = TRUE;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) == SOCKET_ERROR)
    {
        die_socket("setsockopt");
    }

    struct sockaddr_in server_address = {0};
    struct sockaddr_in client_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons((u_short)port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == SOCKET_ERROR)
    {
        die_socket("bind");
    }

    if (listen(server_fd, BACKLOG) == SOCKET_ERROR)
    {
        die_socket("listen");
    }

    printf("Chat created.\n");
    printf("Waiting for client...\n");

    int client_address_len = sizeof(client_address);
    SOCKET client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len);
    if (client_fd == INVALID_SOCKET)
    {
        die_socket("accept");
    }

    printf("Client connected!\n\n");

    if (sodium_init() < 0)
    {
        close_socket(client_fd);
        close_socket(server_fd);
        fprintf(stderr, "sodium_init failed\n");
        exit(EXIT_FAILURE);
    }

    crypto_kx_keys server_keys = {0};
    crypto_session session = {0};
    int result = fill_server_keys(client_fd, &server_keys);
    if (result < 0)
    {
        close_socket(client_fd);
        close_socket(server_fd);
        die(key_exchange_error(result));
    }

    char public_key_hex[crypto_kx_PUBLICKEYBYTES * 2 + 1];
    sodium_bin2hex(public_key_hex, sizeof(public_key_hex), server_keys.pk, sizeof(server_keys.pk));
    printf("Your public key: %s\n", public_key_hex);
    sodium_bin2hex(public_key_hex, sizeof(public_key_hex), server_keys.peer_pk, sizeof(server_keys.peer_pk));
    printf("User public key: %s\n\n", public_key_hex);
    printf("WARNING: Verify peer keys via a trusted channel to prevent MITM attacks!\n\n");

    unsigned char server_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char client_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];

    if (crypto_session_init_tx(&session, server_keys.tx, server_header) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&server_keys, sizeof(server_keys));
        close_socket(client_fd);
        close_socket(server_fd);
        fprintf(stderr, "crypto_session_init_tx failed\n");
        exit(EXIT_FAILURE);
    }

    if (send_all(client_fd, server_header, sizeof(server_header), 0) != 0)
    {
        close_socket(client_fd);
        close_socket(server_fd);
        die_socket("send_all_server_header");
    }

    if (recv_all(client_fd, client_header, sizeof(client_header), 0) != 0)
    {
        close_socket(client_fd);
        close_socket(server_fd);
        die_socket("recv_all_client_header");
    }

    if (crypto_session_init_rx(&session, server_keys.rx, client_header) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&server_keys, sizeof(server_keys));
        close_socket(client_fd);
        close_socket(server_fd);
        fprintf(stderr, "crypto_session_init_rx failed\n");
        exit(EXIT_FAILURE);
    }

    sodium_memzero(&server_keys, sizeof(server_keys));
    run_chat(client_fd, &session);
    crypto_session_clear(&session);
    close_socket(client_fd);
    close_socket(server_fd);
}

void start_client(const struct in_addr *addr, int port)
{
    SOCKET client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_fd == INVALID_SOCKET)
        die_socket("socket");

    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons((u_short)port);
    server_address.sin_addr = *addr;

    if (connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == SOCKET_ERROR)
    {
        die_socket("connect");
    }

    printf("Successfully connected!\n\n");

    if (sodium_init() < 0)
    {
        close_socket(client_fd);
        fprintf(stderr, "sodium_init failed\n");
        exit(EXIT_FAILURE);
    }

    crypto_kx_keys client_keys = {0};
    crypto_session session = {0};
    int result = fill_client_keys(client_fd, &client_keys);
    if (result < 0)
    {
        close_socket(client_fd);
        die(key_exchange_error(result));
    }

    char public_key_hex[crypto_kx_PUBLICKEYBYTES * 2 + 1];
    sodium_bin2hex(public_key_hex, sizeof(public_key_hex), client_keys.pk, sizeof(client_keys.pk));
    printf("Your public key: %s\n", public_key_hex);
    sodium_bin2hex(public_key_hex, sizeof(public_key_hex), client_keys.peer_pk, sizeof(client_keys.peer_pk));
    printf("User public key: %s\n\n", public_key_hex);
    printf("WARNING: Verify peer keys via a trusted channel to prevent MITM attacks!\n\n");

    unsigned char server_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char client_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    if (crypto_session_init_tx(&session, client_keys.tx, client_header) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&client_keys, sizeof(client_keys));
        close_socket(client_fd);
        fprintf(stderr, "crypto_session_init_tx failed\n");
        exit(EXIT_FAILURE);
    }

    if (recv_all(client_fd, server_header, sizeof(server_header), 0) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&client_keys, sizeof(client_keys));
        close_socket(client_fd);
        die_socket("recv_all_server_header");
    }

    if (send_all(client_fd, client_header, sizeof(client_header), 0) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&client_keys, sizeof(client_keys));
        close_socket(client_fd);
        die_socket("send_all_client_header");
    }

    if (crypto_session_init_rx(&session, client_keys.rx, server_header) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&client_keys, sizeof(client_keys));
        close_socket(client_fd);
        fprintf(stderr, "crypto_session_init_rx failed\n");
        exit(EXIT_FAILURE);
    }

    sodium_memzero(&client_keys, sizeof(client_keys));
    run_chat(client_fd, &session);
    crypto_session_clear(&session);
    close_socket(client_fd);
}
