#ifndef CHAT_H
#define CHAT_H

#include "crypto.h"

#include <stddef.h>
#include <winsock2.h>
#include <windows.h>

#define BUFFER_SIZE 4096

typedef struct
{
    char input_buffer[BUFFER_SIZE];
    size_t input_buffer_size;

    CRITICAL_SECTION mutex;

    crypto_session *session;

    SOCKET file_descriptor;
    volatile LONG running;
} Args;

void run_chat(SOCKET file_descriptor, crypto_session *session);

#endif