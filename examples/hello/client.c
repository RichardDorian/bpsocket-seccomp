#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "libbp.h"

int main() {
  struct sockaddr_bp server_addr = {.sbp_family = AF_BP,
                                    .sbp_scheme = BP_SCHEME_IPN,
                                    .sbp_addr = {.sbp_ipn = {1, 150}}};

  int sockfd = socket(AF_BP, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  char message[] = "Hello from the moon!";
  ssize_t bytes_sent =
      sendto(sockfd, &message, sizeof(message), 0,
             (struct sockaddr *)&server_addr, sizeof(server_addr));

  if (bytes_sent == -1) {
    perror("sendto");
    exit(EXIT_FAILURE);
  }

  char buffer[256];
  // We don't care about the server ip so we pass NULL
  ssize_t bytes_read = recvfrom(sockfd, &buffer, sizeof(buffer), 0, NULL, NULL);

  if (bytes_read == -1) {
    perror("recvfrom");
    exit(EXIT_FAILURE);
  }

  puts(buffer);

  return EXIT_SUCCESS;
}
