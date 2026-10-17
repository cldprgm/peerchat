#include "network.h"
#include "crypto.h"
#include "chat.h"

#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <sodium.h>

#define BACKLOG 1

static void die(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

static int recv_all(int fd, unsigned char *buffer, size_t buffer_size, int flags)
{
    ssize_t recv_bytes = 0;
    while (recv_bytes < (ssize_t)buffer_size)
    {
        ssize_t n = recv(fd, buffer + recv_bytes, (ssize_t)buffer_size - recv_bytes, flags);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;

        recv_bytes += n;
    }
    return 0;
}

static int send_all(int fd, unsigned char *buffer, size_t buffer_size, int flags)
{
    ssize_t sent_bytes = 0;
    while (sent_bytes < (ssize_t)buffer_size)
    {
        ssize_t n = send(fd, buffer + sent_bytes, (ssize_t)buffer_size - sent_bytes, flags);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;

        sent_bytes += n;
    }
    return 0;
}

static int fill_server_keys(int fd, crypto_kx_keys *keys)
{
    if (keys == NULL)
        return -1;

    sodium_memzero(keys, sizeof(*keys));

    crypto_kx_keypair(keys->pk, keys->sk);

    // key exchange
    if (send_all(fd, keys->pk, sizeof(keys->pk), 0) < 0)
        return -1;

    if (recv_all(fd, keys->peer_pk, sizeof(keys->peer_pk), 0) < 0)
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

static int fill_client_keys(int fd, crypto_kx_keys *keys)
{
    if (keys == NULL)
        return -1;

    sodium_memzero(keys, sizeof(*keys));

    crypto_kx_keypair(keys->pk, keys->sk);

    // key exchange
    if (recv_all(fd, keys->peer_pk, sizeof(keys->peer_pk), 0) < 0)
        return -2;

    if (send_all(fd, keys->pk, sizeof(keys->pk), 0) < 0)
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

void start_server(int port)
{
    // socket conf
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        die("socket");

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        close(server_fd);
        die("setsockopt");
    }

    struct sockaddr_in client_address = {0};
    struct sockaddr_in server_address = {0};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        close(server_fd);
        die("bind");
    }

    if (listen(server_fd, BACKLOG) < 0)
    {
        close(server_fd);
        die("listen");
    }

    printf("Chat created.\n");
    printf("Waiting for client...\n");

    socklen_t client_address_len = sizeof(client_address);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_len);
    if (client_fd < 0)
    {
        close(server_fd);
        die("accept");
    }

    printf("Client connected!\n\n");

    // crypto_kx (key generation and exchange)
    if (sodium_init() < 0)
    {
        close(client_fd);
        close(server_fd);
        die("sodium_init");
    }

    crypto_kx_keys server_keys = {0};
    crypto_session session = {0};

    int res = fill_server_keys(client_fd, &server_keys);
    if (res < 0)
    {
        const char *err_text = "unknown error";
        if (res == -1)
            err_text = "send_key";
        else if (res == -2)
            err_text = "recv_key";
        else if (res == -3)
            err_text = "suspicious user public key";

        close(client_fd);
        close(server_fd);
        die(err_text);
    }

    // print pb
    char pb_key_hex[crypto_kx_PUBLICKEYBYTES * 2 + 1];
    sodium_bin2hex(pb_key_hex, sizeof(pb_key_hex), server_keys.pk, sizeof(server_keys.pk));
    printf("Your public key: %s\n", pb_key_hex);

    sodium_bin2hex(pb_key_hex, sizeof(pb_key_hex), server_keys.peer_pk, sizeof(server_keys.peer_pk));
    printf("User public key: %s\n\n", pb_key_hex);

    printf("WARNING: Verify peer keys via a trusted channel to prevent MITM attacks!\n\n");

    // crypto_secretstream
    unsigned char server_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char client_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];

    if (crypto_session_init_tx(&session, server_keys.tx, server_header) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&server_keys, sizeof(server_keys));
        close(client_fd);
        close(server_fd);
        die("crypto_session_init_tx");
    }

    if (send_all(client_fd, server_header, sizeof(server_header), 0) != 0)
    {
        close(client_fd);
        close(server_fd);
        die("send_all_server_header");
    }

    if (recv_all(client_fd, client_header, sizeof(client_header), 0) != 0)
    {
        close(client_fd);
        close(server_fd);
        die("recv_all_client_header");
    }

    if (crypto_session_init_rx(&session, server_keys.rx, client_header) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&server_keys, sizeof(server_keys));
        close(client_fd);
        close(server_fd);
        die("crypto_session_init_rx");
    }

    sodium_memzero(&server_keys, sizeof(server_keys));

    run_chat(client_fd, &session);

    crypto_session_clear(&session);

    close(client_fd);
    close(server_fd);
}

void start_client(const struct in_addr *addr, int port)
{
    // socket conf
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0)
        die("socket");

    struct sockaddr_in server_address = {0};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr = *addr;

    if (connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        close(client_fd);
        die("connect");
    }

    printf("Successfully connected!\n\n");

    // crypto_kx (key generation and exchange)
    if (sodium_init() < 0)
    {
        close(client_fd);
        die("sodium_init");
    }

    crypto_kx_keys client_keys = {0};
    crypto_session session = {0};

    int res = fill_client_keys(client_fd, &client_keys);
    if (res < 0)
    {
        const char *err_text = "unknown error";
        if (res == -1)
            err_text = "send_key";
        else if (res == -2)
            err_text = "recv_key";
        else if (res == -3)
            err_text = "suspicious user public key";

        close(client_fd);
        die(err_text);
    }

    // print pb
    char pb_key_hex[crypto_kx_PUBLICKEYBYTES * 2 + 1];
    sodium_bin2hex(pb_key_hex, sizeof(pb_key_hex), client_keys.pk, sizeof(client_keys.pk));
    printf("Your public key: %s\n", pb_key_hex);

    sodium_bin2hex(pb_key_hex, sizeof(pb_key_hex), client_keys.peer_pk, sizeof(client_keys.peer_pk));
    printf("User public key: %s\n\n", pb_key_hex);

    printf("WARNING: Verify peer keys via a trusted channel to prevent MITM attacks!\n\n");

    // crypto_secretstream
    unsigned char server_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char client_header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];

    if (crypto_session_init_tx(&session, client_keys.tx, client_header) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&client_keys, sizeof(client_keys));
        close(client_fd);
        die("crypto_session_init_tx");
    }

    if (recv_all(client_fd, server_header, sizeof(server_header), 0) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&client_keys, sizeof(client_keys));
        close(client_fd);
        die("recv_all_server_header");
    }

    if (send_all(client_fd, client_header, sizeof(client_header), 0) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&client_keys, sizeof(client_keys));
        close(client_fd);
        die("send_all_client_header");
    }

    if (crypto_session_init_rx(&session, client_keys.rx, server_header) != 0)
    {
        crypto_session_clear(&session);
        sodium_memzero(&client_keys, sizeof(client_keys));
        close(client_fd);
        die("crypto_session_init_rx");
    }

    sodium_memzero(&client_keys, sizeof(client_keys));

    run_chat(client_fd, &session);

    crypto_session_clear(&session);

    close(client_fd);
}
