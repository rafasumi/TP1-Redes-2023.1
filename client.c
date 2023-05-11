#include "common.h"
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void usage(const char* bin) {
  eprintf("Usage: %s <server IP address> <server port>\n", bin);
  eprintf("Example IPv4: %s 127.0.0.1 51511\n", bin);
  eprintf("Example IPv6: %s ::1 51511\n", bin);
  exit(EXIT_FAILURE);
}

// Faz o parse do endereço passado como argumento e inicializa um struct do tipo
// sockaddr_storage de acordo com o protocolo adequado
int parse_address(const char* addr_str, const char* port_str,
                  struct sockaddr_storage* storage) {
  if (addr_str == NULL || port_str == NULL) {
    return -1;
  }

  uint16_t port = (uint16_t)atoi(port_str); // unsigned short
  if (port == 0) {
    return -1;
  }
  port = htons(port); // host to network short

  struct in_addr inaddr4; // 32-bit IPv4 address
  if (inet_pton(AF_INET, addr_str, &inaddr4)) {
    struct sockaddr_in* addr4 = (struct sockaddr_in*)storage;
    addr4->sin_family = AF_INET;
    addr4->sin_port = port;
    addr4->sin_addr = inaddr4;
    return 0;
  }

  struct in6_addr inaddr6; // 128-bit IPv6 address
  if (inet_pton(AF_INET6, addr_str, &inaddr6)) {
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)storage;
    addr6->sin6_family = AF_INET6;
    addr6->sin6_port = port;
    memcpy(&(addr6->sin6_addr), &inaddr6, sizeof(inaddr6));
    return 0;
  }

  return -1;
}

int is_valid_extension(const char* ext) {
  for (int i = 0; i < NUM_VALID_EXT; i++) {
    if (strcmp(ext, valid_extensions[i]) == 0)
      return 1;
  }

  return 0;
}

void build_message(char* buffer, const char* selected_file) {
  strcpy(buffer, selected_file);
  char* end = strrchr(buffer, '\0');

  FILE* file_ptr = fopen(selected_file, "r");
  size_t tail_size = strlen("\\end");
  fread(end, 1, BUFFER_SIZE - strlen(buffer) - tail_size, file_ptr);
  fclose(file_ptr);

  char* file_end = strrchr(end, '\0');
  strcpy(file_end, "\\end");
}

int main(int argc, const char* argv[]) {
  if (argc < 3)
    usage(argv[0]);

  struct sockaddr_storage storage;
  if (parse_address(argv[1], argv[2], &storage) != 0) {
    usage(argv[0]);
  }

  int sock;
  sock = socket(storage.ss_family, SOCK_STREAM, 0);
  if (sock == -1) {
    log_exit("socket");
  }

  struct sockaddr* addr = (struct sockaddr*)(&storage);
  if (connect(sock, addr, sizeof(storage)) != 0) {
    log_exit("connect");
  }

  char input[BUFFER_SIZE];
  char selected_file[BUFFER_SIZE];
  memset(selected_file, 0, BUFFER_SIZE);
  char* ptr = NULL;
  while (1) {
    memset(input, 0, BUFFER_SIZE);
    fgets(input, BUFFER_SIZE, stdin);

    // Remove \n do input lido pelo fgets
    input[strcspn(input, "\n")] = '\0';

    if (strcmp(input, "exit") == 0) {
      char message[] = "exit\\end";
      if (send_stream(sock, message, strlen(message)) == -1) {
        log_exit("send");
      }

      char buffer[BUFFER_SIZE];
      memset(buffer, 0, BUFFER_SIZE);
      size_t result = recv_stream(sock, buffer, BUFFER_SIZE);
      if (result == -1) {
        log_exit("recv");
      } else if (result == 0) {
        break;
      }

      printf("%s\n", buffer);

      break;
    } else if (strcmp(input, "send file") == 0) {
      if (selected_file[0] == 0) {
        printf("no file selected!\n");
        continue;
      }

      char buffer[BUFFER_SIZE];
      memset(buffer, 0, BUFFER_SIZE);
      build_message(buffer, selected_file);

      if (send_stream(sock, buffer, strlen(buffer)) == -1) {
        log_exit("send");
      }

      memset(buffer, 0, BUFFER_SIZE);
      size_t result = recv_stream(sock, buffer, BUFFER_SIZE);
      if (result == -1) {
        log_exit("recv");
      } else if (result == 0) {
        break;
      }

      printf("%s\n", buffer);
    }
    // A função strstr encontra a primeira ocorrência de um padrão em uma
    // string. Caso o padrão "select file " for encontrado, então o resto da
    // string deve ser o nome do arquivo.
    else if ((ptr = strstr(input, "select file ")) != NULL) {
      ptr += strlen("select file ");

      // Se não houver nada após "select file ", então é um comando inválido
      if (*ptr == '\0')
        break;

      if (access(ptr, F_OK) != 0) {
        printf("%s does not exist\n", ptr);
        continue;
      }

      char* ext = strrchr(ptr, '.');
      ext++;
      if (!is_valid_extension(ext)) {
        printf("%s not valid!\n", ptr);
        continue;
      }

      strcpy(selected_file, ptr);
      printf("%s selected\n", selected_file);
    } else {
      // Recebeu comando desconhecido, envia "\end" para terminar conexão
      char message[] = "\\end";
      if (send_stream(sock, message, strlen(message)) == -1) {
        log_exit("send");
      }

      break;
    }
  }

  close(sock);

  exit(EXIT_SUCCESS);
}
