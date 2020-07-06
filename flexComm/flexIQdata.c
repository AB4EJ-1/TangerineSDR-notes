/* Copyright (C) 2019 The University of Alabama
* Author: William (Bill) Engelke, AB4EJ
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
* External packages:

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include "de_signals.h"

#define FLEXRADIO_IP "192.168.1.66"
#define FLEX_DISCOVERY_PORT 4992
#define UDPPORT_IN 7791
#define UDPPORT_OUT 7100

struct flexDataSample
  {
  float I_val_int;
  float Q_val_int;
  };

typedef struct flexDataBuf
 {
 char VITA_hdr1[2];  // rightmost 4 bits is a packet counter
 int16_t  VITA_packetsize;
 char stream_ID[4];
 char class_ID[8];
 uint32_t time_stamp;
 uint64_t fractional_seconds;
 struct flexDataSample flexDatSample[512];
 } FLEXBUF;

static struct dataBuf tdataBuf;


static struct sockaddr_in flex_tcp_addr;
static struct sockaddr_in flex_udp_addr;

static int sock, tcpsock;  // sockets
static char flex_reply[3000];

static  int fd;
//static int fd2;
static struct flexDataBuf iqbuffer;


void *receiveFlexAnswers(void *threadid) {

 printf("thread start\n");

while(1==1) {
 int c = recv(sock,flex_reply, 2000,0);
 printf("\n---------------------\n");
  //printf("Flex replied with %i bytes: %s\n",c,flex_reply);
 for(int i = 9; i < 450; i++)
   {
  // if(flex_reply[i] == 0x00) break;
   printf("%c",flex_reply[i]);
   }
 printf("\n---------------------\n");
 }
}


void main () {


 char command1[25] = "c0|client udpport 7791";
 char command2[100] = "c1|stream create daxiq=2 port=7791 ip=192.168.1.207";
 memset(flex_reply, sizeof(flex_reply),0);
 int flexport = UDPPORT_IN;
 sock = socket(AF_INET, SOCK_STREAM, 0);  // set up socket for TCP comm to Flex
 if (sock == -1)
   {
   printf("Error, could not create TCP socket\n");
   }
  else
   printf("TCP socket created\n");

 flex_tcp_addr.sin_addr.s_addr = inet_addr(FLEXRADIO_IP);
 flex_tcp_addr.sin_family      = AF_INET;
 flex_tcp_addr.sin_port        = htons(FLEX_DISCOVERY_PORT);

 pthread_t threadId;
 int rc = pthread_create(&threadId, NULL, &receiveFlexAnswers, NULL);


 if(connect(sock, (struct sockaddr *)&flex_tcp_addr, sizeof(flex_tcp_addr)) < 0)
  {
  printf("Error - could not connect to flex\n");
  }
 else
  {
   printf("Connected...\n");
  sleep(5);
  printf("send: %s\n",command1);
  int c = send(sock,command1, strlen(command1),0);
  printf(" **************** sent %i\n",c);
  sleep(5);
 // c =  recv(sock,flex_reply, 2000,0);
 // printf("********       ************ reply to command: %s\n",flex_reply);
  printf("send: %s\n",command2);
  c = send(sock,command2, strlen(command2),0);
  printf(" xxxxxxxxxxxxxx sent %i\n",c);
  sleep(1);
 // c =  recv(sock,flex_reply, 2000,0);
  //printf("xxxxxxxxxxxx xxxxxxxxx reply to command: %s\n",flex_reply);



  return;


 //int outport = 7100;
 fd_set readfd;
 fd_set writefd;
 int count;
 memset(&tdataBuf,sizeof(tdataBuf),0);

 sock = socket(AF_INET, SOCK_DGRAM, 0);
 if(sock < 0) {
  printf("sock error\n");
  return;
  }

// port for broadcast 
 printf("bcast socket setup...\n");
// fd2 = socket(AF_INET, SOCK_DGRAM, 0);
 int broadcast = 1;
 //struct hostent* he = gethostbyname("192.168.1.255");
 struct hostent* he = gethostbyname("192.168.1.208");
 //int errors = setsockopt(fd2, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(int));

// printf("fd2 setsockopt, rc = %i\n",errors);

/*
    flex_udp_addr.sin_family = AF_INET;
    flex_udp_addrsin_port = htons(UDPPORT_OUT);
    addr.sin_addr = *((struct in_addr*) he->h_addr);
    memset(addr.sin_zero, '\n',sizeof(addr.sin_zero));
*/

 int len =sizeof(iqbuffer);

// port for reading flex
 int addr_len = sizeof(struct sockaddr_in);
 memset((void*)&flex_udp_addr, 0, addr_len);
 flex_udp_addr.sin_family = AF_INET;
 flex_udp_addr.sin_addr.s_addr = htons(INADDR_ANY);
 flex_udp_addr.sin_port = htons(UDPPORT_IN);
 int ret = bind(sock, (struct sockaddr*)&flex_udp_addr, addr_len);
 if (ret < 0){
  printf("bind error\n");
  return;
  }
 FD_ZERO(&readfd);
 FD_SET(sock, &readfd);
 printf("read from port %i\n",UDPPORT_IN);
 int bufcount = 0;
 ret = 1;
 while(1==1) {  // repeating loop
  if(ret > 0){
   if (FD_ISSET(sock, &readfd)){
  //  printf("try read\n");

// each (tangerine)output buffer has the IQ data of exactly 2 (flex)input buffers.
    bufcount = 0;
    for(int j = 0; j <= 1; j++)
     {

      count = recvfrom(sock, &iqbuffer, sizeof(iqbuffer),0, (struct sockaddr*)&flex_udp_addr, &addr_len);
      printf("bytes received = %i\n",count);
      printf("VITA header= %x %x\n",iqbuffer.VITA_hdr1[0],iqbuffer.VITA_hdr1[1]);
      printf("stream ID= %0x %0x %0x %0x\n", iqbuffer.stream_ID[0],iqbuffer.stream_ID[1], iqbuffer.stream_ID[2],iqbuffer.stream_ID[3]);

      printf("timestamp = %i \n",iqbuffer.time_stamp/16777216);
      printf("input buffer %i\n",j);
      tdataBuf.channelCount = 1;
      tdataBuf.centerFreq = 14.999;
      for(int i=0;i<512;i++) {
    //   printf("%f %f \n",iqbuffer.flexDatSample[i].I_val_int,iqbuffer.flexDatSample[i].Q_val_int);
       
       tdataBuf.theDataSample[bufcount].I_val = iqbuffer.flexDatSample[i].I_val_int;
       tdataBuf.theDataSample[bufcount].Q_val = iqbuffer.flexDatSample[i].Q_val_int;
       bufcount++;
     //  if(bufcount >= 512)
      //   bufcount = 0;
     }
    printf("\n");
    }


    printf("broadcast...\n");
  //  int bytes_sent = sendto(fd2, &iqbuffer, len, 0, (struct sockaddr *) &addr, sizeof(addr));
  //  int bytes_sent = sendto(fd2, &tdataBuf, sizeof(tdataBuf), 0, (struct sockaddr *) &addr, sizeof(addr));
  //  printf("bcast done, bytes = %i\n",bytes_sent);
    
  //  FILE * fptr;
  //  fptr = fopen("sampleIQ.dat","wb");
  //  fwrite(&iqbuffer,sizeof(iqbuffer),1,fptr);
    //close(fptr);
    }
  }  // end of repeating loop
 }
 }

 printf("end\n");

}
