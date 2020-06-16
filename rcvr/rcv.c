/*
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <libconfig.h> 
#include <linux/in.h>
#include <sys/types.h>
//#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "de_signals.h"

#define IP_FOUND "IP_FOUND"
#define IP_FOUND_ACK "IP_FOUND_ACK"
#define PORT 7100

static struct dataBuf iqbuffer;

int main() {
  int sock;
  int yes = 1;
  struct sockaddr_in client_addr;
  struct sockaddr_in server_addr;
  int addr_len;
  int count;
  int ret;
  fd_set readfd;
  char buffer[9000];

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("sock error\n");
    return -1;
  }

  addr_len = sizeof(struct sockaddr_in);

  memset((void*)&server_addr, 0, addr_len);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htons(INADDR_ANY);
  server_addr.sin_port = htons(PORT);

  ret = bind(sock, (struct sockaddr*)&server_addr, addr_len);
  if (ret < 0) {
    perror("bind error\n");
    return -1;
  }
  while (1) {
    FD_ZERO(&readfd);
    FD_SET(sock, &readfd);

    ret = select(sock+1, &readfd, NULL, NULL, 0);
    if (ret > 0) {
      if (FD_ISSET(sock, &readfd)) {
        count = recvfrom(sock, &iqbuffer, sizeof(iqbuffer), 0, (struct sockaddr*)&client_addr, &addr_len);
        printf("bytes received:  %i\n",count);
        if (strstr(buffer, IP_FOUND)) {
          printf("\nClient connection information:\n\t IP: %s, Port: %d\n", 
            inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

         //   memcpy(buffer, IP_FOUND_ACK, strlen(IP_FOUND_ACK)+1);
         //   count = sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_len);
        }
      }
    }

  }
}
