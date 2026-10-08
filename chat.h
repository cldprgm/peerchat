#ifndef CHAT_H
#define CHAT_H

#include <stddef.h>
#include <pthread.h>
#include <termios.h>

#define BUFFER_SIZE 2048

typedef struct
{
    char input_buffer[BUFFER_SIZE];
    size_t input_buffer_size;

    pthread_mutex_t mutex;

    struct termios old_terminal;

    int file_descriptor;
    int running;
} Args;

void run_chat(int file_descriptor);

#endif