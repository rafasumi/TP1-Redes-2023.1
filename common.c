#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

char* valid_extensions[NUM_VALID_EXT] = {"txt", "c",   "cpp",
                                         "py",  "tex", "java"};

int send_stream(int socket, char* buffer, size_t len) {
  char* ptr = buffer;
  size_t count;
  while (len) {
    count = send(socket, ptr, len, 0);
    if (count < 0) {
      return -1;
    }

    ptr += count;
    len -= count;
  }

  return 0;
}

int recv_stream(int socket, char* buffer, size_t len) {
  char* ptr = buffer;
  size_t count;
  while (len) {
    count = recv(socket, ptr, len, 0);
    if (count <= 0) {
      return count;
    }

    char* message_end = strstr(buffer, "\\end");
    if (message_end != NULL) {
      memset(message_end, 0, strlen("\\end"));
      break;
    }

    ptr += count;
    len -= count;
  }

  return 1;
}

void log_exit(const char* msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}