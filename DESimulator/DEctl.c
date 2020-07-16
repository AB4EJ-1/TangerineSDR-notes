/* Copyright (C) 2019 The University of Alabama
* Author: William (Bill) Engelke, AB4EJ
* With funding from the Center for Advanced Public Safety and
* The National Science Foundation.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>
#include <fcntl.h>
#include "de_signals.h"
//#define IP_FOUND "IP_FOUND"
//#define IP_FOUND_ACK "IP_FOUND_ACK"
#define PORT 1024

#define UDPPORT 7100

static int LH_port;
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
struct sockaddr_in config_in_addr;

int sock;
int sock1;

static uint16_t LH_CONF_IN_port;  // port C, receives ACK or NAK from config request
static uint16_t LH_CONF_OUT_port; // for sending (outbound) config request to DE

int main() {
 while(1)
 {

 printf("Initialized; await discovery on port %d\n", PORT);

// initialize ports for discovery reeipt and reply

  int addr_len;
  int count;
  int ret;
  DE_CONF_IN_port = 50001;  //fixed port on which to receive config request (CC)
  DE_CH_IN_port = 50002;   // fixed port on which to receive channel setup req. (CH)
  fd_set readfd;
  char buffer[1024];
  sock = socket(AF_INET, SOCK_DGRAM, 0);  // for initial discovery packet
  if (sock < 0) {
    perror("sock error\n");
    return -1;
    }
  sock1 = socket(AF_INET, SOCK_DGRAM, 0);  // for reply via Port B
  if (sock1 < 0) {
    perror("sock1 error\n");
    return -1;
    }
  addr_len = sizeof(struct sockaddr_in);
  memset((void*)&server_addr, 0, addr_len);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htons(INADDR_ANY);
  server_addr.sin_port = htons(PORT);
  cmdport = PORT;  // this could be made to follow randomly chosen port
  cmdport = DE_CONF_IN_port;
  // bind to our port to listen on
  ret = bind(sock, (struct sockaddr*)&server_addr, addr_len);
  if (ret < 0) {
    perror("bind error\n");
    return -1;
  }
// set up for Port B reply
  server_addr.sin_port = htons(DE_CONF_IN_port);
  ret = bind(sock1, (struct sockaddr*)&server_addr, addr_len);
  if (ret < 0) {
    perror("bind error\n");
    return -1;
  }

    FD_ZERO(&readfd);
    FD_SET(sock, &readfd);

    ret = select(sock+1, &readfd, NULL, NULL, 0);
    if (ret > 0) {
      if (FD_ISSET(sock, &readfd)) {
        while(1==1) {
          count = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&client_addr, &addr_len);
          if((buffer[0] & 0xFF) == 0xEF && (buffer[1] & 0xFF) == 0xFE) {
	        fprintf(stderr,"discovery packet detected at startup point\n"); 

            LH_port = ntohs(client_addr.sin_port);

            printf("\nClient connection information:\n\t IP: %s, Port: %d\n", 
            inet_ntoa(client_addr.sin_addr), LH_port);
	        buffer[10] = 0x07;

	        count = sendto(sock1, buffer, 60, 0, (struct sockaddr*)&client_addr,
		            sizeof(client_addr));
         }
        }
      }
    }


 printf("DE exited, rc = %i; closing sockets & threads\n", r);
 close(sock);
 close(sock1);


 printf("attempting restart\n");
 }

}

