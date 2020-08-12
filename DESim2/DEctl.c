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

//#define  DE_CONF_IN 50001  //fixed port on which to receive config request (CC)
//#define  DE_CH_IN   50002   // fixed port on which to receive channel setup req. (CH)

#define UDPPORT 7100

static int LH_port;
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
struct sockaddr_in config_in_addr;

int sock;
int sock1;
//int cmdport;

//static uint16_t LH_CONF_IN_port;  // port C, receives ACK or NAK from config request
//static uint16_t LH_CONF_OUT_port; // for sending (outbound) config request to DE
//static uint16_t DE_CONF_IN_port;  // port B ; DE listens for config request on this port
//static uint16_t DE_CH_IN_port;    // port D; DE listens channel setup on this port

int main() {
 printf("Starting DEctl\n");
 while(1)
 {

 printf("Initialized; await discovery on port %d\n", PORT);

// initialize ports for discovery reeipt and reply

  int addr_len;
  int count;
  int ret;
  char syscommand_start[100] = "";
  char syscommand_kill[100]  = "killall -9 ./DEmain2";
//  DE_CONF_IN_port = DE_CONF_IN;  //fixed port on which to receive config request (CC)
 // DE_CH_IN_port = DE_CH_IN;   // fixed port on which to receive channel setup req. (CH)
  fd_set readfd;
  char buffer[1024];
  printf("set up sock \n");
  sock = socket(AF_INET, SOCK_DGRAM, 0);  // for initial discovery packet
  if (sock < 0) {
    perror("sock error\n");
    return -1;
    }
/*
  printf("set up sock1\n");
  sock1 = socket(AF_INET, SOCK_DGRAM, 0);  // for reply via Port B
  if (sock1 < 0) {
    perror("sock1 error\n");
    return -1;
    }
*/
  addr_len = sizeof(struct sockaddr_in);
  memset((void*)&server_addr, 0, addr_len);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htons(INADDR_ANY);
  server_addr.sin_port = htons(PORT);
  //cmdport = PORT;  // this could be made to follow randomly chosen port
 // cmdport = DE_CONF_IN_port;
  // bind to our port to listen on
  printf("bind to sock\n");
  ret = bind(sock, (struct sockaddr*)&server_addr, addr_len);
  if (ret < 0) {
    perror("bind error\n");
    return -1;
  }
// set up for Port B reply
 // server_addr.sin_port = htons(DE_CONF_IN_port);
 // printf("bind to sock1\n");
 // ret = bind(sock1, (struct sockaddr*)&server_addr, addr_len);
  if (ret < 0) {
    perror("bind error\n");
    return -1;
  }

    FD_ZERO(&readfd);
    FD_SET(sock, &readfd);

    fprintf(stderr,"entering loop\n");
    while(1==1)
    {
    printf("select\n");
    ret = select(sock+1, &readfd, NULL, NULL, 0);

     if (ret > 0) {
       if (FD_ISSET(sock, &readfd)) {
  
          printf("READ port 1024\n");
          count = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&client_addr, &addr_len);
          if((buffer[0] & 0xFF) == 0xEF && (buffer[1] & 0xFF) == 0xFE) {
	        fprintf(stderr,"discovery packet came from %s port %i; killing old process; \n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
            system(syscommand_kill);
            //sleep(5);
            printf("starting DEmain\n");
            

            LH_port = ntohs(client_addr.sin_port);

          //  printf("\nClient connection information:\n\t IP: %s, Port: %d\n", 
          //  inet_ntoa(client_addr.sin_addr), LH_port);


  sock1 = socket(AF_INET, SOCK_DGRAM, 0);  // for reply via Port B
  if (sock1 < 0) {
    perror("sock1 error\n");
    return -1;
    }

          printf("DEctl: Send Discovery reply to: IP: %s, Port: %d\n", 
            inet_ntoa(client_addr.sin_addr), LH_port);
	      buffer[10] = 0x07;
// temp test of sending from Port B
	      count = sendto(sock1, buffer, 60, 0, (struct sockaddr*)&client_addr,
		            sizeof(client_addr));
          printf("bytes sent: %i\n",count);
          int sa_len = sizeof(server_addr);
          
          if(getsockname(sock1,(struct sockaddr *)&server_addr,&sa_len) == -1)
           printf("getsockname failed\n");
          printf("Local port# = %d\n",(int)ntohs(server_addr.sin_port));
          sprintf(syscommand_start,"./DEmain2 %i %s %i &",(int)ntohs(server_addr.sin_port),inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            printf("command = %s \n",syscommand_start);

// set up for Port B reply
  //server_addr.sin_port = htons(DE_CONF_IN_port);
 // printf("bind to sock1\n");
 // ret = bind(sock1, (struct sockaddr*)&server_addr, addr_len);

//fprintf(stderr,"after discovery reply sock1 IP=%s port %i; \n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));


          close(sock1);  // may have to re-establish this later
          system(syscommand_start);

	      //  buffer[10] = 0x07;
// discovery reply packet
	      //  count = sendto(sock1, buffer, 60, 0, (struct sockaddr*)&client_addr,		            sizeof(client_addr));
         }
        }
      }
    }


 printf("DE exited\n");
 close(sock);
 close(sock1);


 printf("attempting restart\n");
 }

}

