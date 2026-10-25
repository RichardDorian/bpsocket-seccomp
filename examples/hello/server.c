#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "libbp.h"

int main() {
  int sockfd = socket(AF_BP, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_bp addr = {.sbp_family = AF_BP,
                             .sbp_scheme = BP_SCHEME_IPN,
                             .sbp_addr = {.sbp_ipn = {1, 150}}};

  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  // No need to listen, BP only supports SOCK_DGRAM and is connection less

  char buffer[256];
  struct sockaddr_bp client_addr;
  socklen_t client_addr_len;
  while (1) {
    recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr,
             &client_addr_len);

    // Print the client's address
    if (client_addr.sbp_scheme == BP_SCHEME_IPN) {
      printf("Coming from ipn:%i.%i\n", client_addr.sbp_addr.sbp_ipn.node,
             client_addr.sbp_addr.sbp_ipn.service);
    } else if (client_addr.sbp_scheme == BP_SCHEME_DTN) {
      printf("Coming from: dtn:%s\n", client_addr.sbp_addr.sbp_dtn);
    }

    // Write the buffer to the standard output
    puts(buffer);

    // Send back a message to the client
    char message[] = "Hello from the ISS!";
    sendto(sockfd, &message, sizeof(message), 0,
           (struct sockaddr *)&client_addr, sizeof(client_addr));
  }
}
