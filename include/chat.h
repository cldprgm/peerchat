#ifndef CHAT_H
#define CHAT_H

#include "crypto.h"

#include <stddef.h>
#include <pthread.h>
#include <termios.h>

#define BUFFER_SIZE 4096

typedef struct
{
    char input_buffer[BUFFER_SIZE];
    size_t input_buffer_size;

    pthread_mutex_t mutex;

    struct termios old_terminal;

    crypto_session *session;

    int file_descriptor;
    int running;
} Args;

void run_chat(int file_descriptor, crypto_session *session);

#endif