#ifndef CRYPTO_H
#define CRYPTO_H

#include <sodium.h>

typedef struct
{
    crypto_secretstream_xchacha20poly1305_state tx;
    crypto_secretstream_xchacha20poly1305_state rx;

    int tx_ready;
    int rx_ready;
} crypto_session;

int crypto_session_init_tx(
    crypto_session *session,
    const unsigned char key[crypto_kx_SESSIONKEYBYTES],
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES]);

int crypto_session_init_rx(
    crypto_session *session,
    const unsigned char key[crypto_kx_SESSIONKEYBYTES],
    const unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES]);

void crypto_session_clear(crypto_session *session);

#endif