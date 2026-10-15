#include "crypto.h"

int crypto_session_init_tx(
    crypto_session *session,
    const unsigned char key[crypto_kx_SESSIONKEYBYTES],
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES])
{
    if (session == NULL || key == NULL || header == NULL)
        return -1;

    if (crypto_secretstream_xchacha20poly1305_init_push(&session->tx, header, key) != 0)
        return -1;

    session->tx_ready = 1;
    return 0;
}

int crypto_session_init_rx(
    crypto_session *session,
    const unsigned char key[crypto_kx_SESSIONKEYBYTES],
    const unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES])
{
    if (session == NULL || key == NULL || header == NULL)
        return -1;

    if (crypto_secretstream_xchacha20poly1305_init_pull(&session->rx, header, key) != 0)
        return -1;

    session->rx_ready = 1;
    return 0;
}

void crypto_session_clear(crypto_session *session)
{
    if (session != NULL)
        sodium_memzero(session, sizeof(*session));
}