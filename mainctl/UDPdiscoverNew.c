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
 
  int sock;
  int yes = 1;
  struct sockaddr_in broadcast_addr;

  struct sockaddr_in DE_addr[16];

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
  broadcast_addr.sin_port = htons(PORT);
// build OpenHPSDR discovery packet
  outbound_buffer[0] = 0xEF;
  outbound_buffer[1] = 0xFE;
  outbound_buffer[2] = 0x02;
  struct timeval tv;  // timevalue for socket timeout
  tv.tv_sec = 1;  // so we can look for multiple devices in discovery
  tv.tv_usec = 0; // and quit when all have responded
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

  for (i=0;i<2;i++) {
    ret = sendto(sock, outbound_buffer, 63, 0, (struct sockaddr*) &broadcast_addr, addr_len);

    FD_ZERO(&readfd);
    FD_SET(sock, &readfd);

    ret = select(sock + 1, &readfd, NULL, NULL, NULL);

    if (ret > 0) {
    int j = 0;
      if (FD_ISSET(sock, &readfd)) {
		  while(1)
          {
		puts("reading now...");
        count = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&broadcast_addr, &addr_len);
		printf("Bytes received = %d \n",count);
		if(count < 0) 
		{
 		  puts("no data");
		  printf("Discovery found %d devices.\n",j);
		  puts("discovery done");
		  return 0;
		}		
        printf("\trecvmsg is '%s'\n", buffer);
        DE_addr[i] = broadcast_addr;
    
        printf("\tfound DE, IP is %s, Port is %d\n", inet_ntoa(broadcast_addr.sin_addr),htons(broadcast_addr.sin_port));
		j++;

/*   all the below is for testing, sending a console input to the discovered device
		  printf("input?\n");
		  char *line = NULL;
		  size_t len = 0;
		  ssize_t read;
		  read = getline(&line, &len, stdin);
		  printf("%s\n", line);


     //    ret = sendto(sock, line, len, 0, (struct sockaddr*) &broadcast_addr, addr_len);
         ret = sendto(sock, line, len, 0, (struct sockaddr*) &DE_addr[0], addr_len);
		 printf("send ret = %d\n", ret);
*/

		 } // while 1
        
      } // if FD_ISSET
    } // if ret 0

  } // for i
}
