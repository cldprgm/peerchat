#include "crypto.h"

#define BUFFER_SIZE 4096

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

int encrypt_message(
    crypto_session *session,
    const unsigned char *plaintext,
    unsigned long long plaintext_len,
    unsigned char *ciphertext,
    unsigned long long *ciphertext_len)
{
    if (session == NULL ||
        plaintext == NULL ||
        ciphertext == NULL ||
        ciphertext_len == NULL ||
        !session->tx_ready)
    {
        return -1;
    }

    *ciphertext_len = 0;

    if (plaintext_len > crypto_secretstream_xchacha20poly1305_MESSAGEBYTES_MAX)
        return -1;

    if (plaintext_len + crypto_secretstream_xchacha20poly1305_ABYTES > BUFFER_SIZE)
        return -1;

    if (crypto_secretstream_xchacha20poly1305_push(
            &session->tx,
            ciphertext,
            ciphertext_len,
            plaintext,
            plaintext_len,
            NULL,
            0,
            crypto_secretstream_xchacha20poly1305_TAG_MESSAGE) != 0)
    {
        return -1;
    }

    return 0;
}

int decrypt_message(
    crypto_session *session,
    const unsigned char *ciphertext,
    unsigned long long ciphertext_len,
    unsigned char *plaintext,
    unsigned long long *plaintext_len)
{
    if (session == NULL ||
        ciphertext == NULL ||
        plaintext == NULL ||
        plaintext_len == NULL ||
        !session->rx_ready)
    {
        return -1;
    }

    *plaintext_len = 0;

    if (ciphertext_len < crypto_secretstream_xchacha20poly1305_ABYTES)
        return -1;

    if (ciphertext_len > crypto_secretstream_xchacha20poly1305_MESSAGEBYTES_MAX + crypto_secretstream_xchacha20poly1305_ABYTES)
        return -1;

    if (ciphertext_len > BUFFER_SIZE)
        return -1;

    unsigned char tag;

    if (crypto_secretstream_xchacha20poly1305_pull(
            &session->rx,
            plaintext,
            plaintext_len,
            &tag,
            ciphertext,
            ciphertext_len,
            NULL,
            0) != 0)
    {
        return -1;
    }

    if (tag != crypto_secretstream_xchacha20poly1305_TAG_MESSAGE)
    {
        sodium_memzero(plaintext, (size_t)*plaintext_len);
        *plaintext_len = 0;
        return -1;
    }

    return 0;
}