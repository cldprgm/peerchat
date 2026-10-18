#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "crypto.h"
#include "test_support.h"

static void test_encrypt_decrypt_round_trip(void)
{
    unsigned char key[crypto_kx_SESSIONKEYBYTES] = {0};
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char plaintext[] = "unit test message";
    unsigned char ciphertext[4096];
    unsigned char decrypted[4096];
    unsigned long long ciphertext_len = 0;
    unsigned long long decrypted_len = 0;
    crypto_session sender = {0};
    crypto_session receiver = {0};

    assert(crypto_session_init_tx(&sender, key, header) == 0);
    assert(crypto_session_init_rx(&receiver, key, header) == 0);
    assert(encrypt_message(&sender, plaintext, sizeof(plaintext) - 1,
                           ciphertext, &ciphertext_len) == 0);
    assert(decrypt_message(&receiver, ciphertext, ciphertext_len,
                           decrypted, &decrypted_len) == 0);
    assert(decrypted_len == sizeof(plaintext) - 1);
    assert(memcmp(decrypted, plaintext, decrypted_len) == 0);

    crypto_session_clear(&sender);
    crypto_session_clear(&receiver);
}

static void test_empty_message_round_trip(void)
{
    unsigned char key[crypto_kx_SESSIONKEYBYTES] = {0};
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char empty_plaintext = 0;
    unsigned char ciphertext[crypto_secretstream_xchacha20poly1305_ABYTES];
    unsigned char decrypted[1];
    unsigned long long ciphertext_len = 0;
    unsigned long long decrypted_len = 0;
    crypto_session sender = {0};
    crypto_session receiver = {0};

    assert(crypto_session_init_tx(&sender, key, header) == 0);
    assert(crypto_session_init_rx(&receiver, key, header) == 0);
    assert(encrypt_message(&sender, &empty_plaintext, 0, ciphertext,
                           &ciphertext_len) == 0);
    assert(ciphertext_len == crypto_secretstream_xchacha20poly1305_ABYTES);
    assert(decrypt_message(&receiver, ciphertext, ciphertext_len, decrypted,
                           &decrypted_len) == 0);
    assert(decrypted_len == 0);

    crypto_session_clear(&sender);
    crypto_session_clear(&receiver);
}

static void test_reject_invalid_session_initialization_arguments(void)
{
    unsigned char key[crypto_kx_SESSIONKEYBYTES] = {0};
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_session session = {0};

    assert(crypto_session_init_tx(NULL, key, header) == -1);
    assert(crypto_session_init_tx(&session, NULL, header) == -1);
    assert(crypto_session_init_tx(&session, key, NULL) == -1);
    assert(crypto_session_init_rx(NULL, key, header) == -1);
    assert(crypto_session_init_rx(&session, NULL, header) == -1);
    assert(crypto_session_init_rx(&session, key, NULL) == -1);
}

static void test_reject_invalid_encrypt_arguments_and_oversized_message(void)
{
    unsigned char key[crypto_kx_SESSIONKEYBYTES] = {0};
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char plaintext[1] = {0};
    unsigned char ciphertext[4096];
    unsigned long long ciphertext_len = 17;
    crypto_session session = {0};

    assert(crypto_session_init_tx(&session, key, header) == 0);
    assert(encrypt_message(NULL, plaintext, sizeof(plaintext), ciphertext,
                           &ciphertext_len) == -1);
    assert(encrypt_message(&session, NULL, sizeof(plaintext), ciphertext,
                           &ciphertext_len) == -1);
    assert(encrypt_message(&session, plaintext, sizeof(plaintext), NULL,
                           &ciphertext_len) == -1);
    assert(encrypt_message(&session, plaintext, sizeof(plaintext), ciphertext,
                           NULL) == -1);

    ciphertext_len = 17;
    assert(encrypt_message(&session, plaintext,
                           crypto_secretstream_xchacha20poly1305_MESSAGEBYTES_MAX + 1ULL,
                           ciphertext, &ciphertext_len) == -1);
    assert(ciphertext_len == 0);

    crypto_session_clear(&session);
}

static void test_reject_tampered_ciphertext(void)
{
    unsigned char key[crypto_kx_SESSIONKEYBYTES] = {0};
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char plaintext[] = "authenticated payload";
    unsigned char ciphertext[4096];
    unsigned char decrypted[4096];
    unsigned long long ciphertext_len = 0;
    unsigned long long decrypted_len = 1;
    crypto_session sender = {0};
    crypto_session receiver = {0};

    assert(crypto_session_init_tx(&sender, key, header) == 0);
    assert(crypto_session_init_rx(&receiver, key, header) == 0);
    assert(encrypt_message(&sender, plaintext, sizeof(plaintext) - 1,
                           ciphertext, &ciphertext_len) == 0);
    ciphertext[0] ^= 1;

    assert(decrypt_message(&receiver, ciphertext, ciphertext_len, decrypted,
                           &decrypted_len) == -1);
    assert(decrypted_len == 0);

    crypto_session_clear(&sender);
    crypto_session_clear(&receiver);
}

static void test_reject_invalid_decrypt_arguments_and_short_ciphertext(void)
{
    unsigned char key[crypto_kx_SESSIONKEYBYTES] = {0};
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    unsigned char ciphertext[crypto_secretstream_xchacha20poly1305_ABYTES] = {0};
    unsigned char plaintext[1];
    unsigned long long plaintext_len = 17;
    crypto_session session = {0};

    assert(crypto_session_init_rx(&session, key, header) == 0);
    assert(decrypt_message(NULL, ciphertext, sizeof(ciphertext), plaintext,
                           &plaintext_len) == -1);
    assert(decrypt_message(&session, NULL, sizeof(ciphertext), plaintext,
                           &plaintext_len) == -1);
    assert(decrypt_message(&session, ciphertext, sizeof(ciphertext), NULL,
                           &plaintext_len) == -1);
    assert(decrypt_message(&session, ciphertext, sizeof(ciphertext), plaintext,
                           NULL) == -1);

    plaintext_len = 17;
    assert(decrypt_message(&session, ciphertext,
                           crypto_secretstream_xchacha20poly1305_ABYTES - 1,
                           plaintext, &plaintext_len) == -1);
    assert(plaintext_len == 0);

    crypto_session_clear(&session);
}

static void test_clear_session_zeroes_state(void)
{
    crypto_session session;
    memset(&session, 0xA5, sizeof(session));
    crypto_session_clear(&session);

    const unsigned char *bytes = (const unsigned char *)&session;
    for (size_t i = 0; i < sizeof(session); ++i)
        assert(bytes[i] == 0);
    crypto_session_clear(NULL);
}

int main(void)
{
    unsigned passed = 0;
    unsigned total = 0;

    assert(sodium_init() >= 0);
    RUN_TEST(test_encrypt_decrypt_round_trip, passed, total);
    RUN_TEST(test_empty_message_round_trip, passed, total);
    RUN_TEST(test_reject_invalid_session_initialization_arguments, passed, total);
    RUN_TEST(test_reject_invalid_encrypt_arguments_and_oversized_message, passed, total);
    RUN_TEST(test_reject_tampered_ciphertext, passed, total);
    RUN_TEST(test_reject_invalid_decrypt_arguments_and_short_ciphertext, passed, total);
    RUN_TEST(test_clear_session_zeroes_state, passed, total);

    puts("crypto.c unit tests passed");
    PRINT_TEST_RESULT(passed, total);
    return 0;
}
