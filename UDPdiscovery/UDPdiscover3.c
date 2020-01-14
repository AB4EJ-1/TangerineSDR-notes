#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <string.h>
#include <errno.h>

#define IP_FOUND "IP_FOUND"
#define IP_FOUND_ACK "IP_FOUND_ACK"
#define PORT 1024

int main() {
  puts("starting");
  int sock;
  int yes = 1;
  struct sockaddr_in broadcast_addr;
//  struct sockaddr_in DE_addr;
  int addr_len;
  int count;
  int ret;
  fd_set readfd;
  char buffer[1024];
  char outbound_buffer[63];
  int i;
  
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("sock error");
    return -1;
  }
  ret = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(yes));
  if (ret == -1) {
    perror("setsockopt error");
    return 0;
  }
  memset(outbound_buffer,0,sizeof(outbound_buffer));

  addr_len = sizeof(struct sockaddr_in);

  memset((void*)&broadcast_addr, 0, addr_len);
  broadcast_addr.sin_family = AF_INET;
  broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  broadcast_addr.sin_port = htons(PORT);  // PORT

  outbound_buffer[0] = 0xEF;
  outbound_buffer[1] = 0xFE;
  outbound_buffer[2] = 0x02;


  for (i=0;i<2;i++) {
    ret = sendto(sock, outbound_buffer, 63, 0, (struct sockaddr*) &broadcast_addr, addr_len);

          printf("after bcast, IP is %s, Port is %d\n", inet_ntoa(broadcast_addr.sin_addr),htons(broadcast_addr.sin_port));

    FD_ZERO(&readfd);
    FD_SET(sock, &readfd);

    ret = select(sock + 1, &readfd, NULL, NULL, NULL);

          printf("after bcast, IP is %s, Port is %d\n", inet_ntoa(broadcast_addr.sin_addr),htons(broadcast_addr.sin_port));

    if (ret > 0) {
      if (FD_ISSET(sock, &readfd)) {
		//  while(1)
          {  // the 4th arg is flags; normally set here to 0
        count = recvfrom(sock, buffer, 1024, 1032, (struct sockaddr*)&broadcast_addr, &addr_len);
        printf("\trecvmsg is '%s'\n", buffer);
      //  if (strstr(buffer, IP_FOUND_ACK)) {
          printf("\tfound server IP is %s, Port is %d\n", inet_ntoa(broadcast_addr.sin_addr),htons(broadcast_addr.sin_port));
/*
		  printf("input?\n");
		  char *line = NULL;
		  size_t len = 0;
		  ssize_t read;
		  read = getline(&line, &len, stdin);
		  printf("%s\n", line);
*/
		//  if(line[0] == 51 && line[1] == 51) return 0; // QQ halts it

    //     outbound_buffer[0] = line[0];
    //     outbound_buffer[1] = line[1];

         ret = sendto(sock, outbound_buffer, 60, 0, (struct sockaddr*) &broadcast_addr, addr_len);
		 printf("send ret = %d\n", ret);
		 }


        
      }
    }

  }
}
