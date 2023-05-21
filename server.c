#include "common.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PENDING 10

// Retirado das aulas do professor Ítalo.
void usage(const char* bin) {
  eprintf("Usage: %s <v4|v6> <server port>\n", bin);
  eprintf("Example: %s v4 51511\n", bin);
  exit(EXIT_FAILURE);
}

// Inicializa o objeto sockaddr_storage com base no protocolo informado como
// argumento. Retorna 0 quando há sucesso e -1 caso contrário. Retirado das
// aulas do professor Ítalo.
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

// Casa a string "str" com alguma das extensões no array de extensões válidas.
// Se houver casamento, retorna um ponteiro para a posição após "str". Caso
// contrário, retorna NULL.
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

// Função auxiliar para fazer o parse do arquivo recebido, assumindo que a
// mensagem recebida representa um arquivo. Em caso de sucesso, cria ou atualiza
// o arquivo e retorna 0. Caso contrário, retorna -1.
int parse_file(const char* message, char* response) {
  // Procura a última ocorrência do caractere '.'
  char* header_end = strrchr(message, '.');
  // Se não há '.' na mensagem, é inválida
  if (header_end == NULL) {
    return -1;
  }

  header_end = match_extension(header_end);
  // Se não casa com nenhuma das extensões, é inválida
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

  // Verifica se o arquivo presente no cabeçalho existe para determinar a
  // resposta adequada
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

  // Inicializa o objeto sockaddr_storage para dar bind em todos os endereços
  // IP associados à interface
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
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) {
    log_exit("setsockopt");
  }

  struct sockaddr* addr = (struct sockaddr*)(&storage);
  if (bind(server_sock, addr, sizeof(storage)) != 0) {
    log_exit("bind");
  }

  if (listen(server_sock, MAX_PENDING) != 0) {
    log_exit("listen");
  }

  struct sockaddr_storage client_storage;
  struct sockaddr* client_addr = (struct sockaddr*)(&client_storage);
  socklen_t client_addrlen = sizeof(client_storage);

  int client_sock = accept(server_sock, client_addr, &client_addrlen);
  if (client_sock == -1) {
    log_exit("accept");
  }

  char buffer[BUFFER_SIZE];
  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    if (recv_stream(client_sock, buffer, BUFFER_SIZE) <= 0) {
      log_exit("recv");
    }

    if (strcmp(buffer, "exit") == 0) {
      // Se a mensagem recebida é "exit", então deve fechar a conexão e
      // finalizar a execução

      printf("%s\n", buffer);
      char response[] = "connection closed\\end";

      if (send(client_sock, response, strlen(response), 0) != strlen(response)) {
        log_exit("send");
      }

      break;
    } else {
      // Se a mensagem não é exit, tenta fazer parse como se fosse uma mensagem
      // com arquivo. Se o parse falhar, trata como comando desconhecido e
      // finaliza a execução

      char response[BUFFER_SIZE];
      if (parse_file(buffer, response) == 0) {
        if (send(client_sock, response, strlen(response), 0) != strlen(response)) {
          log_exit("send");
        }
      } else {
        break;
      }
    }
  }

  close(client_sock);
  close(server_sock);

  exit(EXIT_SUCCESS);
}
