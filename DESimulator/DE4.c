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


static int LH_port;
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;
int sock;
long cmdthreadID;
int cmdport;
int stoplink;
int stopData;
int stopft8;
int ft8active;
struct iqpair {
  float ival; 
  float qval;
};

/*
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
*/

void *sendFT8(void *threadid) {
   struct iqpair iqpairdat[240000];
   FILE *fp;
   char name[64];
   double dialfreq;
   struct dataBuf ft8Buffer;
   struct dataSample ft8Sample;
   ft8active = 1;
   long bufcount = 0;
   ssize_t sentBytes;

   printf("stating ft8 data transfer; thread id = %d \n",threadid);

   printf("starting\n");
   if(threadid == 0)
      strcpy(name, "ft8_0_7075500_1_191106_2236.c2");
   if((int)threadid == 1)
      strcpy(name, "ft8_1_10137500_1_191106_2236.c2");
   if(threadid == 2)
      strcpy(name, "ft8_2_14075500_1_191106_2236.c2");
   if(threadid == 3)
      strcpy(name, "ft8_3_18101500_1_191106_2236.c2");
   if(threadid == 4)
      strcpy(name, "ft8_4_21075500_1_191106_2236.c2");
   if(threadid == 5)
     strcpy(name, "ft8_5_24916500_1_191106_2236.c2");
   if(threadid == 6)
     strcpy(name, "ft8_6_28075500_1_191106_2236.c2");
   if(threadid == 7)
     strcpy(name, "ft8_7_50314500_1_191106_2236.c2");
   if((fp = fopen(name, "r")) == NULL)
    {
      fprintf(stderr, "Cannot open ft8 input file %s.\n", name);
      return 1;
    }
   fread(&dialfreq, 1, 8, fp);
   printf("%f\n",dialfreq);
   size_t s = fread(iqpairdat,sizeof(iqpairdat),1,fp);
   printf("read done, bytes = %ld\n", s);
   time_t epoch = time(NULL);
   printf("unix time = %ld\n", epoch);
   strncpy(ft8Buffer.bufType,"FT",2);
   ft8Buffer.timeStamp = (double) epoch;
   client_addr.sin_port = htons(LH_port);
   ft8Buffer.centerFreq = dialfreq;
   ft8Buffer.channelNo = (int)threadid;
//   while(1)
   {
     long inputCounter = 0;
     for(int i=0; i< 60; i++)  // once per second
     {
       for(int j=0; j < 4; j++)  // send 4 buffers, each containing 1000 complex samples
       {
         for (int k=0; k < 1000; k++)  
         {
         ft8Buffer.theDataSample[k].I_val = iqpairdat[inputCounter].ival;
         ft8Buffer.theDataSample[k].Q_val = iqpairdat[inputCounter].qval;
         inputCounter++;
         }
         printf("i = %d, j = %d, inputCounter = %ld\n", i, j,  inputCounter);
         ft8Buffer.bufCount = bufcount++;

         sentBytes = sendto(sock, (const struct dataBuf *)&ft8Buffer, sizeof(ft8Buffer), 0, 
	      (struct sockaddr*)&client_addr, sizeof(client_addr));

         fprintf(stderr,"UDP message sent from thread. bytes= %ld\n", sentBytes); 
    // sleep(1);
         usleep(250000);  // wait for this many microseconds
         if(stopft8)
	       {
           puts("UDP thread end");
           ft8active = 0;
           stopft8 = 0;
	       pthread_exit(NULL);
	       }
       }
     }

   }
   ft8active = 0;  // done here
 
}

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
    strncpy(myBuffer.bufType,"RG",2);
	myBuffer.timeStamp = (double) epoch;
	for (int i = 0; i < 1024; i++) {
	  I =  sin ( (double)i * 2.0 * 3.1415926535897932384626433832795 / 1024.0);
	  Q =  cos ( (double)i * 2.0 * 3.1415926535897932384626433832795 / 1024.0);
	  myBuffer.theDataSample[i].I_val = I;
	  myBuffer.theDataSample[i].Q_val = Q;
           }

  ssize_t sentBytes;
  long loopstart;
  loopstart = clock();

  while(1)
  { 
   // puts("UDP thread start; hit sem_wait");
    //sem_wait(&mutex);
   // puts("passed wait");
    myBuffer.bufCount = bufcount++;

    client_addr.sin_port = htons(LH_port);
    sentBytes = sendto(sock, (const struct dataBuf *)&myBuffer, sizeof(myBuffer), 0, 
	   (struct sockaddr*)&client_addr, sizeof(client_addr));

    fprintf(stderr,"UDP message sent from thread. bytes= %ld\n", sentBytes); 
    // sleep(1);
    usleep(528);  // wait for this many microseconds
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
  stopft8 = 0;
  ft8active = 0;
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
    if(strncmp(buffer, "SF",2)==0)
      {
      if (ft8active)
        {
        puts("FT8 already running");
        continue;
        }
      printf("Start FT8 command received\n");
      char cmdline[200];
      strncpy(cmdline, buffer, count);  // get command info 
      printf("cmdline = %s\n",cmdline);
      char *pch;
      char thecmd[2];
      int channel[8];
      float ft8freq[8];
      sscanf(cmdline,"%s %d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f",
         thecmd, &channel[0],&channel[1],&channel[2],&channel[3],
         &channel[4],&channel[5],&channel[6],&channel[7],
         &ft8freq[0],&ft8freq[1],&ft8freq[2],&ft8freq[3],
         &ft8freq[4],&ft8freq[5],&ft8freq[6],&ft8freq[7]);
      printf("conversion = %s %d %d %d %d %d %d %d %d %f %f %f %f %f %f %f %f \n",
         thecmd, channel[0],channel[1],channel[2],channel[3],
         channel[4],channel[5],channel[6],channel[7],
         ft8freq[0],ft8freq[1],ft8freq[2],ft8freq[3],
         ft8freq[4],ft8freq[5],ft8freq[6],ft8freq[7]);

      pthread_t ft8nthread[8];  
      int k;
      int rc;    // create one thread for each ft8 channel to run
      for(int ft8chan = 0; ft8chan < 8; ft8chan++)
       {
         if(channel[ft8chan] == -1) continue;  // bypass any channels turned off
         k = ft8chan;
         rc = pthread_create(&ft8nthread[ft8chan], NULL, sendFT8, (void*)k);
         printf("startng ft8 channel %d %d \n",ft8chan,rc);
       }
      stopft8 = 0;
   //   int j = 2;
  //    pthread_t ft8thread;
   //   int rc = pthread_create(&ft8thread, NULL, sendFT8, (void*)j);
      continue;
      }
    if(strncmp(buffer, "XF",2)==0)
      {
      printf("Stop FT8 command received\n");
      stopft8 = 1;
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
