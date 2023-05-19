#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

// Array de extensões válidas. A extensão "cpp" precisa ser definida antes que a
// extensão "c" no arquivo, pois, caso contrário, nenhum arquivo "cpp" é
// reconhecido pelo servidor.
char* valid_extensions[NUM_VALID_EXT] = {"txt", "cpp", "c", "py", "tex", "java"};

// Função usada para implementar uma versão mais "confiável" do recv.
// Continuamente chama "recv" até que a sequência "\end" esteja contida no
// buffer. Isso é necessário já que não é possível saber com antecedência qual é
// a quantidade de bytes sendo enviada
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
      // Remove a sequência "\end" do buffer
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