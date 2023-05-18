#include "common.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PENDING 10

void usage(const char* bin) {
  eprintf("Usage: %s <v4|v6> <server port>\n", bin);
  eprintf("Example: %s v4 51511\n", bin);
  exit(EXIT_FAILURE);
}

// Inicializa o objeto sockaddr_storage com base no protocolo informado como
// argumento
int sockaddr_init(const char* protocol, const char* port_str,
                  struct sockaddr_storage* storage) {
  uint16_t port = (uint16_t)atoi(port_str); // unsigned short
  if (port == 0) {
    return -1;
  }
  port = htons(port); // host to network short

  memset(storage, 0, sizeof(*storage));

  if (strcmp(protocol, "v4") == 0) {
    struct sockaddr_in* addr4 = (struct sockaddr_in*)storage;
    addr4->sin_family = AF_INET;
    addr4->sin_addr.s_addr = INADDR_ANY;
    addr4->sin_port = port;
  } else if (strcmp(protocol, "v6") == 0) {
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)storage;
    addr6->sin6_family = AF_INET6;
    addr6->sin6_addr = in6addr_any;
    addr6->sin6_port = port;
  } else {
    return -1;
  }

  return 0;
}

char* match_extension(const char* str) {
  char* match;
  for (int i = 0; i < NUM_VALID_EXT; i++) {
    match = strstr(str, valid_extensions[i]);
    if (match != NULL) {
      match += strlen(valid_extensions[i]);
      break;
    }
  }

  return match;
}

int parse_file(const char* message, char* response) {
  char* header_end = strchr(message, '.');
  if (header_end == NULL) {
    return -1;
  }

  header_end = match_extension(header_end);
  if (header_end == NULL) {
    return -1;
  }

  size_t header_size = header_end - message;
  size_t content_size = strlen(message) - header_size;

  char* file_name = malloc(header_size);
  strncpy(file_name, message, header_size);
  file_name[header_size] = '\0';

  char* file_content = malloc(content_size);
  strcpy(file_content, header_end);
  file_content[content_size] = '\0';

  if (access(file_name, F_OK) == 0) {
    sprintf(response, "file %s overwritten\\end", file_name);
  } else {
    sprintf(response, "file %s received\\end", file_name);
  }

  FILE* file_ptr = fopen(file_name, "w");
  fputs(file_content, file_ptr);
  fclose(file_ptr);

  free(file_name);
  free(file_content);

  return 0;
}

int main(int argc, const char* argv[]) {
  if (argc < 3)
    usage(argv[0]);

  struct sockaddr_storage storage;
  if (sockaddr_init(argv[1], argv[2], &storage) != 0) {
    usage(argv[0]);
  }

  int server_sock;
  server_sock = socket(storage.ss_family, SOCK_STREAM, 0);
  if (server_sock == -1) {
    log_exit("socket");
  }

  // Opção que permite reaproveitar um socket em uso
  int enable = 1;
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) !=
      0) {
    log_exit("setsockopt");
  }

  struct sockaddr* addr = (struct sockaddr*)(&storage);
  if (bind(server_sock, addr, sizeof(storage)) != 0) {
    log_exit("bind");
  }

  if (listen(server_sock, MAX_PENDING) != 0) {
    log_exit("listen");
  }

  char buffer[BUFFER_SIZE];
  struct sockaddr_storage client_storage;
  struct sockaddr* client_addr = (struct sockaddr*)(&client_storage);
  socklen_t client_addrlen = sizeof(client_storage);

  int client_sock = accept(server_sock, client_addr, &client_addrlen);
  if (client_sock == -1) {
    log_exit("accept");
  }

  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    if (recv_stream(client_sock, buffer, BUFFER_SIZE) <= 0) {
      log_exit("recv");
    }

    if (strcmp(buffer, "exit") == 0) {
      printf("%s\n", buffer);
      char response[] = "connection closed\\end";

      if (send_stream(client_sock, response, strlen(response)) <= 0) {
        log_exit("send");
      }

      break;
    } else {
      char response[BUFFER_SIZE];
      if (parse_file(buffer, response) == 0) {
        if (send_stream(client_sock, response, strlen(response)) <= 0) {
          log_exit("send");
        }
      } else {
        // Mensagem não tinha formato válido, portanto era um comando
        // desconhecido
        break;
      }
    }
  }

  close(client_sock);
  close(server_sock);

  exit(EXIT_SUCCESS);
}
