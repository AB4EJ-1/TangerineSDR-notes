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

//#define IP_FOUND "IP_FOUND"
//#define IP_FOUND_ACK "IP_FOUND_ACK"
#define PORT 1024


static int LH_port;
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
int sock;
long cmdthreadID;
int cmdport;
int stoplink;
int stopData;

// notional buffer for DE A/D output. Will update with the real thing later...
struct dataSample
	{
	float I_val;
	float Q_val;
	};

struct dataBuf
	{
	long bufcount;
	long timeStamp;
	struct dataSample myDataSample[1024];
	};


///// Data acquisition (ring buffer or firehose) simulation thread ////////////////////////
void *sendData(void *threadid) {
  int addr_len;
  //memset((void*)&server_addr, 0, addr_len);
  puts("starting thread");
// build an example DE data buffer containing a low freq sine wave
	float I;
	float Q;
	float A;
	A = 1.0;
	long bufcount = 0;
	struct dataBuf myBuffer;
	struct dataSample mySample;
	time_t epoch = time(NULL);
	printf("unix time = %ld\n", epoch);

	for (int i = 0; i < 1024; i++) {
	  I =  sin ( (double)i * 2.0 * 3.1415926535897932384626433832795 / 1024.0);
	  Q =  cos ( (double)i * 2.0 * 3.1415926535897932384626433832795 / 1024.0);
	  myBuffer.timeStamp = (double) epoch;
	  myBuffer.myDataSample[i].I_val = I;
	  myBuffer.myDataSample[i].Q_val = Q;
           }

  ssize_t sentBytes;
  long loopstart;
  loopstart = clock();

  while(1)
  { 
   // puts("UDP thread start; hit sem_wait");
    //sem_wait(&mutex);
   // puts("passed wait");
    myBuffer.bufcount = bufcount++;

    client_addr.sin_port = htons(LH_port);
    sentBytes = sendto(sock, (const struct dataBuf *)&myBuffer, sizeof(myBuffer), 0, 
	   (struct sockaddr*)&client_addr, sizeof(client_addr));

    fprintf(stderr,"UDP message sent from thread. bytes= %ld\n", sentBytes); 
    sleep(1);
   // usleep(528);  // wait for this many microseconds
    if(stopData)
	{
         puts("UDP thread end");
	 pthread_exit(NULL);
	}
	
  //  sem_post(&mutex);
  }

  printf("sending data took ~ %zd  microsec\n", clock() - loopstart);

  puts("ending thread");

}

void discoveryReply(char buffer[1024]) {
  fprintf(stderr,"discovery packet detected\n"); 
  buffer[10] = 0x07;
  LH_port = ntohs(client_addr.sin_port);
	 
  printf("\nDiscovery 2, Client connection information:\n\t IP: %s, Port: %d\n", 
             inet_ntoa(client_addr.sin_addr), LH_port);
  int count = sendto(sock, buffer, 60, 0, (struct sockaddr*)&client_addr,
		 sizeof(client_addr));
}

///////////////////////////////////////////////////////////////////////////////////
int main() {

  stoplink = 0;
  stopData = 0;
  int addr_len;
  int count;
  int ret;
  fd_set readfd;
  char buffer[1024];
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
  cmdport = PORT;  // this could be made to follow randomly chosen port
  // bind to our port to listen on
  ret = bind(sock, (struct sockaddr*)&server_addr, addr_len);
  if (ret < 0) {
    perror("bind error\n");
    return -1;
  }
  while (1) {
  
  printf("Initialized; await discovery on port %d\n", PORT);

    FD_ZERO(&readfd);
    FD_SET(sock, &readfd);

    ret = select(sock+1, &readfd, NULL, NULL, 0);
    if (ret > 0) {
      if (FD_ISSET(sock, &readfd)) {
        count = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&client_addr, &addr_len);
        if((buffer[0] & 0xFF) == 0xEF && (buffer[1] & 0xFF) == 0xFE) {
	      fprintf(stderr,"discovery packet detected at startup point\n"); 
          LH_port = ntohs(client_addr.sin_port);
          printf("\nClient connection information:\n\t IP: %s, Port: %d\n", 
            inet_ntoa(client_addr.sin_addr), LH_port);
	      buffer[10] = 0x07;

	      count = sendto(sock, buffer, 60, 0, (struct sockaddr*)&client_addr,
		            sizeof(client_addr));
        }
      }
    }

  puts("Now starting command processing loop");

  /////////////////////////////// control loop ////////////////////
  while(1)
    {
    printf("awaiting command\n");
    count = recvfrom(sock, buffer, cmdport , 0, (struct sockaddr*)&client_addr, &addr_len);
   // LH_port = ntohs(client_addr.sin_port);
    printf("command recd %c%c %x02 %x02 from port %d\n",buffer[0],buffer[1],buffer[0],buffer[1], LH_port);

   // command processsing

    if(strncmp(buffer, "S?",2) == 0 )
	{
    
	printf("STATUS INQUIRY\n");

    client_addr.sin_port = htons(LH_port);  // this may wipe desired port


    count = sendto(sock, "OK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
    printf("response = %d  sent to ",count);
    printf(" IP: %s, Port: %d\n", 
    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	continue;
	}
    if(strncmp(buffer, "UL",2) == 0)
	{  // future function for allowing LH to drop its link to this DE
	printf("stoplink\n");
	stoplink = 1;
	continue;
	}
    if(strncmp(buffer, "XC",2)==0)
	{
	printf("Main loop stopping data acquisition\n");
	stopData = 1;
	continue;
	}
    if(strncmp(buffer, "XX",2)==0)
	{
	printf("HALTING\n");
	return 0;
	}
    if(strncmp(buffer, "SC",2)==0)
	{
	puts("starting sendData");
	stopData = 0;
  	int j = 1;
  	pthread_t datathread;
  	int rc = pthread_create(&datathread, NULL, sendData, (void *)j);
  	printf("thread start rc = %d\n",rc);
    continue;
	}
  // in case we are running but get another discovery packet
  // This essentially switches DE simulator to talk to a different LH and/or port.
  // TODO: Need to keep track of multiple LH devices, allow link & unlink
    if((buffer[0] & 0xFF) == 0xEF && (buffer[1] & 0xFF) == 0xFE) {
	discoveryReply(buffer);
	continue;
	}
 
     ////////////////////////////////////////////////

    // finally  - if command not recognized...
    puts("unknown command");
    count = sendto(sock, "NAK", 3, 0, (struct sockaddr*)&client_addr, addr_len);
    printf("response = %d\n",count);

    }  // end of control loop

  } // end of discovery loop
}
