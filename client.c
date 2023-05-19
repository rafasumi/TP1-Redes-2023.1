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
// sockaddr_storage de acordo com o protocolo adequado. Retorna 0 quando há
// sucesso e -1 caso contrário.
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

// Função auxiliar usada para verificar se uma extensão é válida. Basicamente,
// procura pela extensão no array de extensões válidas conhecidas.
int is_valid_extension(const char* ext) {
  for (int i = 0; i < NUM_VALID_EXT; i++) {
    if (strcmp(ext, valid_extensions[i]) == 0)
      return 1;
  }

  return 0;
}

// Função auxiliar usada para construir a mensagem para enviar um arquivo na
// rede. Concatena o título com o conteúdo e, depois, com a sequência "\end".
void build_message(char* buffer, const char* selected_file) {
  strcpy(buffer, selected_file);
  char* end = strrchr(buffer, '\0');

  FILE* file_ptr = fopen(selected_file, "r");
  fread(end, 1, BUFFER_SIZE - strlen(buffer) - strlen("\\end"), file_ptr);
  fclose(file_ptr);

  char* file_end = strrchr(end, '\0');
  strcpy(file_end, "\\end");
}

int main(int argc, const char* argv[]) {
  if (argc < 3)
    usage(argv[0]);

  // Faz o parse do endereço recebido como parâmetro
  struct sockaddr_storage storage;
  if (parse_address(argv[1], argv[2], &storage) != 0) {
    usage(argv[0]);
  }

  // Cria um novo socket no endereço
  int sock;
  sock = socket(storage.ss_family, SOCK_STREAM, 0);
  if (sock == -1) {
    log_exit("socket");
  }

  // Abre uma nova conexão no socket criado
  struct sockaddr* addr = (struct sockaddr*)(&storage);
  if (connect(sock, addr, sizeof(storage)) != 0) {
    // A conexão pode falhar caso o servidor não esteja ouvindo
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
      // Se o input é "exit", envia a mensagem "exit\end" para o servidor e
      // aguarda resposta. Depois de receber resposta, finaliza a execução.

      char message[] = "exit\\end";
      if (send(sock, message, strlen(message), 0) != strlen(message)) {
        log_exit("send");
      }

      char buffer[BUFFER_SIZE];
      memset(buffer, 0, BUFFER_SIZE);
      if (recv_stream(sock, buffer, BUFFER_SIZE) <= 0) {
        log_exit("recv");
      }

      printf("%s\n", buffer);

      break;
    } else if (strcmp(input, "send file") == 0) {
      // Se o input é "send file", tenta enviar o arquivo seleciado caso tenha
      // algum.

      if (selected_file[0] == 0) {
        printf("no file selected!\n");
        continue;
      }

      char buffer[BUFFER_SIZE];
      memset(buffer, 0, BUFFER_SIZE);
      build_message(buffer, selected_file);

      if (send(sock, buffer, strlen(buffer), 0) != strlen(buffer)) {
        log_exit("send");
      }

      memset(buffer, 0, BUFFER_SIZE);
      if (recv_stream(sock, buffer, BUFFER_SIZE) <= 0) {
        log_exit("recv");
      }

      printf("%s\n", buffer);
    }
    // A função strstr encontra a primeira ocorrência de um padrão em uma
    // string. Caso o padrão "select file " for encontrado, então o resto da
    // string provavelmente é o nome do arquivo.
    else if ((ptr = strstr(input, "select file ")) != NULL) {
      ptr += strlen("select file ");

      // Se não houver nada após "select file ", trata como um comando inválido
      if (*ptr == '\0') {
        char message[] = "\\end";
        if (send(sock, message, strlen(message), 0) != strlen(message)) {
          log_exit("send");
        }

        break;
      }

      // Verifica se o arquivo informado existe
      if (access(ptr, F_OK) != 0) {
        printf("%s does not exist\n", ptr);
        continue;
      }

      // Valida a extensão do arquivo
      char* ext = strrchr(ptr, '.');
      ext++;
      if (!is_valid_extension(ext)) {
        printf("%s not valid!\n", ptr);
        continue;
      }

      strcpy(selected_file, ptr);
      printf("%s selected\n", selected_file);
    } else {
      // Se o input passado não cai em nenhum dos casos anteriores, então é um
      // comando desconhecido. Nesse caso, envia "\end" para servidor para que
      // a conexão seja terminada
      char message[] = "\\end";
      if (send(sock, message, strlen(message), 0) <= 0) {
        log_exit("send");
      }

      break;
    }
  }

  close(sock);

  exit(EXIT_SUCCESS);
}
