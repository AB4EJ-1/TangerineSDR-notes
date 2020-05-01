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
struct sockaddr_in config_in_addr;

int sock;
int sock1;
int sock2;
long cmdthreadID;
int cmdport;
int stoplink;
int stopData;
int stopft8;
int ft8active;
int config_busy;
struct iqpair {
  float ival; 
  float qval;
};

union {
  char mybuf1[100];
  CONFIGBUF myConfigBuf;
       } d;

  union {
    char configBuffer[1024];
    CHANNELBUF chBuf;
    } cb ;

/*
union {
  char mybuf1[100];
  CONFIGBUF myConfigBuf;
       } LH;
*/

static uint16_t LH_CONF_IN_port;  // port C, receives ACK or NAK from config request
static uint16_t LH_CONF_OUT_port; // for sending (outbound) config request to DE
static uint16_t DE_CONF_IN_port;  // port B ; DE listens for config request on this port
static uint16_t LH_DATA_IN_port;  // port F; LH listens for spectrum data on this port
static uint16_t DE_CH_IN_port;    // port D; DE listens channel setup on this port
static uint16_t LH_DATA_OUT_port; // for sending (outbound) data (e.g., mic audio) to DE


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

   printf("stating ft8 data transfer; thread id = %p \n",threadid);

   printf("starting\n");
   if(threadid == 0)
      strcpy(name, "ft8_0_7075500_1_191106_2236.c2");
   if((long)threadid == 1)
      strcpy(name, "ft8_1_10137500_1_191106_2236.c2");
   if((long)threadid == 2)
      strcpy(name, "ft8_2_14075500_1_191106_2236.c2");
   if((long)threadid == 3)
      strcpy(name, "ft8_3_18101500_1_191106_2236.c2");
   if((long)threadid == 4)
      strcpy(name, "ft8_4_21075500_1_191106_2236.c2");
   if((long)threadid == 5)
     strcpy(name, "ft8_5_24916500_1_191106_2236.c2");
   if((long)threadid == 6)
     strcpy(name, "ft8_6_28075500_1_191106_2236.c2");
   if((long)threadid == 7)
     strcpy(name, "ft8_7_50314500_1_191106_2236.c2");
   if((fp = fopen(name, "r")) == NULL)
    {
      fprintf(stderr, "Cannot open ft8 input file %s.\n", name);
      return (void *) 1;
    }
   fread(&dialfreq, 1, 8, fp);
   printf("%f\n",dialfreq);
   size_t s = fread(iqpairdat,sizeof(iqpairdat),1,fp);
   printf("read done, bytes = %ld\n", s);
   time_t epoch = time(NULL);
   printf("unix time = %ld\n", epoch);
   strncpy(ft8Buffer.bufType,"FT",2);
   ft8Buffer.timeStamp = (double) epoch;
 //  client_addr.sin_port = htons(LH_port); 
 //  client_addr.sin_port = htons(d.myConfigBuf.dataPort);
   client_addr.sin_port = htons(LH_DATA_IN_port);
   ft8Buffer.centerFreq = dialfreq;
   ft8Buffer.channelNo = (long)threadid;
//   while(1)
   {
     long inputCounter = 0;
     for(int i=0; i< 60; i++)  // once per second
     {
       for(int j=0; j < 4; j++)  // send 4 buffers, each containing 1000 complex samples
       {
         for(int k=0; k < 1000; k++)
         {
         ft8Buffer.theDataSample[k].I_val = iqpairdat[inputCounter].ival;
         ft8Buffer.theDataSample[k].Q_val = iqpairdat[inputCounter].qval;
         inputCounter++;
         }
         printf("i = %d, j = %d, inputCounter = %ld\n", i, j,  inputCounter);
         ft8Buffer.dval.bufCount = bufcount++;

         sentBytes = sendto(sock, (const struct dataBuf *)&ft8Buffer, sizeof(ft8Buffer), 0, 
	      (struct sockaddr*)&client_addr, sizeof(client_addr));

         fprintf(stderr,"UDP message sent from thread to port %u. bytes= %ld\n", 
           htons(client_addr.sin_port), sentBytes); 
    // sleep(1);
         usleep(250000);  // wait for this many microseconds
         printf("stopft8=%i \n",stopft8);
         if(stopft8)
	       {
           puts("UDP thread end");
           ft8active = 0;

	       pthread_exit(NULL);

	       }
       }
     }

   }
   ft8active = 0;  // done here
}

/*
void *awaitCH(void *threadid) {
  printf("Starting await-CH thread listening on port %d\n",DE_CH_IN_port);
  printf("... will send data to LH port %d\n",LH_DATA_IN_port);
  char configBuffer[1024];
  CHANNELBUF chBuf;

}
*/

// This thread handles incoming CH  channel setup
void *awaitConfig(void *threadid) {
  config_busy = 1;
  printf("Starting await-config thread listening on port %d (Port D)\n",DE_CH_IN_port);
//  printf("... will send data to LH port %d\n",LH_DATA_IN_port);

  int addr_len;
  int ret;
  int count;
  int optval;
// create & bind socket for inbound config packets
  fd_set readcfg;
  sock2 = socket(AF_INET,SOCK_DGRAM,0);
  setsockopt(sock2, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
  if(sock2 < 0 ) { perror("sock2 error\n"); return (void *) -1; }
  addr_len = sizeof(struct sockaddr_in);
  memset((void*)&config_in_addr,0,addr_len);
  config_in_addr.sin_family = AF_INET;
  config_in_addr.sin_addr.s_addr = htons(INADDR_ANY);
  config_in_addr.sin_port = htons(DE_CH_IN_port);  // listen on Port D
  ret = bind(sock2,(struct sockaddr *)&config_in_addr,addr_len);
  if(ret < 0) { perror("sock2 bind error\n"); return (void *) -1; }
  
  while(1)
    {
    count = recvfrom(sock2, cb.configBuffer, sizeof(cb.configBuffer) , 0, 
        (struct sockaddr*)&config_in_addr, &addr_len);

    printf("CHANNEL Setup CH received %s\n",cb.chBuf.chCommand);
    int actChannels = cb.chBuf.activeChannels;
    int dataRate = cb.chBuf.channelDatarate;
    printf("active channels: %i, rate = %i\n",actChannels,dataRate);
    for (int i=0; i<actChannels; i++) 
      {
      if(cb.chBuf.channelDef[i].antennaPort == -1)  // means this channel is off
        continue;
      else
        printf("%i, Channel %i, Port %i, Freq %lf\n", i, cb.chBuf.channelDef[i].channelNo, 
        cb.chBuf.channelDef[i].antennaPort, cb.chBuf.channelDef[i].channelFreq);
      }
  
    client_addr.sin_port = htons(LH_CONF_IN_port ); 

    count = sendto(sock, "AK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
    printf("response = %u bytes sent to LH port %u \n ",count, LH_CONF_IN_port) ;
    }
}

///// Data acquisition (ring buffer or firehose) simulation thread ////////////////////////
void *sendData(void *threadid) {
  int addr_len;
  int noOfChannels = 0;
  //memset((void*)&server_addr, 0, addr_len);
  printf("starting thread, to send to LH port %d, using following channel layout:\n",LH_DATA_IN_port);

    for (int i=0; i<16; i++) 
      {
      if(cb.chBuf.channelDef[i].antennaPort == -1)  // means this channel is off
        continue;
      else
        printf("%i, Channel %i, Port %i, Freq %lf\n", i, cb.chBuf.channelDef[i].channelNo, 
        cb.chBuf.channelDef[i].antennaPort, cb.chBuf.channelDef[i].channelFreq);
        noOfChannels++;
      }
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
    myBuffer.channelCount = noOfChannels;
    int sampleCount = 1024 / noOfChannels;
    printf("Active channels: %i \n",noOfChannels);
    printf("Sanple count per buffer: %i \n", sampleCount);

/*    original code for outputting single channel
	for (int i = 0; i < 1024; i++) {
	  I =  sin ( (double)i * 2.0 * 3.1415926535897932384626433832795 / 1024.0);
	  Q =  cos ( (double)i * 2.0 * 3.1415926535897932384626433832795 / 1024.0);
	  myBuffer.theDataSample[i].I_val = I;
	  myBuffer.theDataSample[i].Q_val = Q;
           }
*/
	for (int i = 0; i < (sampleCount * noOfChannels); i=i+noOfChannels) {
     for(int j=0;j<noOfChannels;j++) {
      // here the float j produces different frequency for each channel
	  I =  sin ( (double)i * (float)(j+1) * 3.1415926535897932384626433832795 / (double)(sampleCount));
	  Q =  cos ( (double)i * (float)(j+1) * 3.1415926535897932384626433832795 / (double)(sampleCount));
	  myBuffer.theDataSample[i+j].I_val = I;
	  myBuffer.theDataSample[i+j].Q_val = Q;
           }
      }
  ssize_t sentBytes;
  long loopstart;
  loopstart = clock();

  while(1)
  { 
   // puts("UDP thread start; hit sem_wait");
    //sem_wait(&mutex);
   // puts("passed wait");
    myBuffer.dval.bufCount = bufcount++;

    //client_addr.sin_port = htons(LH_port);
 //   client_addr.sin_port = htons(d.myConfigBuf.dataPort);
   
    client_addr.sin_port = htons(LH_DATA_IN_port);
    sentBytes = sendto(sock, (const struct dataBuf *)&myBuffer, sizeof(myBuffer), 0, 
	   (struct sockaddr*)&client_addr, sizeof(client_addr));

    fprintf(stderr,"UDP message sent from thread to port %u. bytes= %ld\n", 
           LH_DATA_IN_port, sentBytes); 
    sleep(1);
    //usleep(528);  // wait for this many microseconds
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
  config_busy = 0;
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
      //    client_addr.sin_port = ntohs(DE_CONF_IN_port);  // temp test
          LH_port = ntohs(client_addr.sin_port);

          printf("\nClient connection information:\n\t IP: %s, Port: %d\n", 
            inet_ntoa(client_addr.sin_addr), LH_port);
	      buffer[10] = 0x07;
// temp test of sending from Port B
	      count = sendto(sock1, buffer, 60, 0, (struct sockaddr*)&client_addr,
		            sizeof(client_addr));
        }
      }
    }

  puts("Now starting command processing loop");

  /////////////////////////////// control loop ////////////////////
  while(1)
    {
    printf("awaiting command to come in from port %u\n", cmdport);
    count = recvfrom(sock1, buffer, cmdport , 0, (struct sockaddr*)&client_addr, &addr_len);
   // LH_port = ntohs(client_addr.sin_port);
    printf("command recd %c%c %x %x %x %x from port %d, bytes=%d\n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4],buffer[5], LH_port,count);
    char bufstr[50];
    strncpy(bufstr, buffer, count);
    printf("Raw buf= %s\n",bufstr);
   // command processsing

    if(strncmp(buffer, "S?",2) == 0 )
	  { 
	  printf("STATUS INQUIRY\n");
      client_addr.sin_port = htons(LH_port);  // this may wipe desired port

      count = sendto(sock1, "OK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
      printf("response = %d  sent to ",count);
      printf(" IP: %s, Port: %d\n", 
      inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	  continue;
	  }
    if(strncmp(buffer, CONFIG_CHANNELS,2) == 0)
      {
      puts("CONFIG_CHANNEL received at wrong port");
      continue;
      }

    if(strncmp(buffer, "R?",2) == 0)   // Request for list of data rates
     {
     printf("Request for Data Rates\n");
     DATARATEBUF myDataRateBuf = {0};
     strncpy(myDataRateBuf.buftype, "DR",2);
     myDataRateBuf.dataRate[0].rateNumber= 1;
     myDataRateBuf.dataRate[0].rateValue = 8;
     myDataRateBuf.dataRate[1].rateNumber= 2;
     myDataRateBuf.dataRate[1].rateValue = 4000;
     myDataRateBuf.dataRate[2].rateNumber= 3;
     myDataRateBuf.dataRate[2].rateValue = 8000;
     myDataRateBuf.dataRate[3].rateNumber= 4;
     myDataRateBuf.dataRate[3].rateValue = 48000;
     myDataRateBuf.dataRate[4].rateNumber= 5;
     myDataRateBuf.dataRate[4].rateValue = 96000;
     myDataRateBuf.dataRate[5].rateNumber= 6;
     myDataRateBuf.dataRate[5].rateValue = 192000;
     myDataRateBuf.dataRate[6].rateNumber= 7;
     myDataRateBuf.dataRate[6].rateValue = 384000;
     myDataRateBuf.dataRate[7].rateNumber= 8;
     myDataRateBuf.dataRate[7].rateValue = 768000;
     myDataRateBuf.dataRate[8].rateNumber= 9;
     myDataRateBuf.dataRate[8].rateValue = 1536000;
     client_addr.sin_port = htons(LH_CONF_IN_port);

      count = sendto(sock1, (const struct datarateBuf *)&myDataRateBuf, sizeof(DATARATEBUF), 0, (struct sockaddr*)&client_addr, addr_len);
      printf("response = %d  sent to ",count);
      printf(" IP: %s, Port: %d\n", 
      inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	  continue;
     }


    if(strncmp(buffer, CREATE_CHANNEL ,2) == 0)
      {

  //    CONFIGBUF myConfigBuf;

      memcpy(d.mybuf1, buffer, 6);

      printf("CREATE CHANNEL RECD, cmd = %s, port C = %hu, port F = %hu \n",
          d.myConfigBuf.cmd, d.myConfigBuf.configPort, d.myConfigBuf.dataPort);
  //    if(config_busy)  // the channels already configured
   //     {
   //      puts("Command channel already set up; ignoring");
  //       continue;
  //      }
      LH_CONF_IN_port = d.myConfigBuf.configPort;
    
      LH_DATA_IN_port = d.myConfigBuf.dataPort;
      client_addr.sin_port = htons(LH_port);  // this may wipe desired port

      strncpy(d.myConfigBuf.cmd, "AK", 2);
      d.myConfigBuf.configPort = DE_CONF_IN_port;
      d.myConfigBuf.dataPort = DE_CH_IN_port;  // this is Port E (mic data) currently unsupported

      long c = 100;   // thread ID for config thread
      long ch = 200;  // thread ID for ch thread
 //     pthread_t configthread;
  //    int rc = pthread_create(&configthread, NULL, awaitConfig, (void *)c);

      count = sendto(sock1, d.mybuf1, sizeof(d.myConfigBuf), 0, (struct sockaddr*)&client_addr, addr_len);
      printf("response = %d  sent to ",count);
      printf(" IP: %s, Port: %d\n", 
      inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

      if(config_busy)  // the channels already configured
        {
         puts("Command channel already set up; ignoring");
         continue;
        }

// start thread for receiving config request (CC)
      pthread_t configthread;
      int rc = pthread_create(&configthread, NULL, awaitConfig, (void *)c);

/*
      pthread_t chthread;
      rc = pthread_create(&chthread, NULL, awaitCH, (void *)ch);
*/


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
      long k;
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
  	  long j = 1;
  	  pthread_t datathread;
  	  int rc = pthread_create(&datathread, NULL, sendData, (void *)j);
  	  printf("thread start rc = %d\n",rc);
	  printf("SEND ACK to port %u\n",LH_CONF_IN_port);
      client_addr.sin_port = htons(LH_CONF_IN_port);  // this may wipe desired port
      count = sendto(sock1, "AK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
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
