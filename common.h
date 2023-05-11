#ifndef COMMON_H
#define COMMON_H

#include <unistd.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#define BUFFER_SIZE 500

#define NUM_VALID_EXT 6
extern char* valid_extensions[];

int send_stream(int socket, char* buffer, size_t len);
int recv_stream(int socket, char* buffer, size_t len);

void log_exit(const char* msg);

#endif