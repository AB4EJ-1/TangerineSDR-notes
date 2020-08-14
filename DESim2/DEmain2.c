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
#define FLEXFT_IN 7790
#define FLEXDATA_IN 7791

struct flexDataSample
  {
  float I_val;
  float Q_val;
  };

typedef struct flexDataBuf
 {
 char VITA_hdr1[2];  // rightmost 4 bits is a packet counter
 int16_t  VITA_packetsize;
 char stream_ID[4];
 char class_ID[8];
 uint32_t time_stamp;
 uint64_t fractional_seconds;
 struct flexDataSample flexDataSample[512];
 } FLEXBUF;
struct sockaddr_in flex_addr;
static  int fd;

static struct dataBuf iqbuffer;
static struct flexDataBuf iqbuffer2;
static struct VITAdataBuf iqbuffer2_in;
static struct VITAdataBuf iqbuffer2_out;
static struct VITAdataBuf ft8buffer_out[4];

static int LH_port;
static int LH_IP;
//static char[15];
struct sockaddr_in client_addr;
struct sockaddr_in client_addr2;
struct sockaddr_in server_addr;
struct sockaddr_in config_in_addr;
struct sockaddr_in portF_addr;

int sock;
int sockft8out;
int sock1;

int sock3;
int sock5;
int sock6;
static int sock4;
long cmdthreadID;
static int CCport;
//int cmdport;
int stoplink;
int stopData;
int stopDataColl;
int stopft8;
int ft8active;
int config_busy;
int noOfChannels;
int dataRate;  // rate at which activated channel runs
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



static uint16_t LH_CONF_IN_port[3];  // port C, receives ACK or NAK from config request
static uint16_t LH_CONF_OUT_port[3]; // for sending (outbound) config request to DE
//static uint16_t DE_CONF_IN_portB;  // port B ; DE listens for CC on this port
static uint16_t LH_DATA_IN_port[3];  // port F; LH listens for spectrum data on this port
static uint16_t DE_CH_IN_port[3] = {50001,50002,50003};    // port D; DE listens channel setup on this port
static uint16_t LH_DATA_OUT_port ; // for sending (outbound) data (e.g., mic audio) to DE (unused)

/////////////////////////////////////////////////////////////////////
void *sendFT8flex(void * threadid){

  fd_set readfd;
  int count;
  int streamNo = 0;
  ft8active = 1;
  printf("in Flex FT8 thread; init sock4\n");
  sock4 = socket(AF_INET, SOCK_DGRAM, 0);
  printf("after socket assign, sock4= %i\n",sock4);
  if(sock4 < 0) {
    printf("sock4 error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
    }

  int yes = 1;  // make socket re-usable
  if(setsockopt(sock4, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
   printf("sock4: Error setting sock option SO_REUSEADDR\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
   }

  printf("sock4 created\n");
  int addr_len = sizeof(struct sockaddr_in);
  memset((void*)&flex_addr, 0, addr_len);
  flex_addr.sin_family = AF_INET;
  flex_addr.sin_addr.s_addr = htons(INADDR_ANY);
  flex_addr.sin_port = htons(FLEXFT_IN);
  printf("bind sock4\n:");
  int ret = bind(sock4, (struct sockaddr*)&flex_addr, addr_len);
  if (ret < 0){
    printf("bind error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
     }
  FD_ZERO(&readfd);
  FD_SET(sock4, &readfd);
  printf("in flex FT8 thread read from port %i\n",FLEXFT_IN);
 // client_addr.sin_port = htons(LH_DATA_IN_port);
// temporary hard code for ft8 port testing
  client_addr2.sin_family = AF_INET;
  client_addr2.sin_addr.s_addr = client_addr.sin_addr.s_addr;
  client_addr2.sin_port = htons(40003);
 ret = 1;

// note that this clears all copies of ft8buffer_out
 memset(&ft8buffer_out,0,sizeof(ft8buffer_out));
 uint64_t samplecount = 0;  // number of IQ samples processed from input
 uint64_t totaloutputbuffercount[4];  // number of buffers sent
 uint64_t totalinputsamplecount[4];
 int outputbuffercount[4];
 for (int i = 0; i < 4; i++)
   {
   outputbuffercount[i] = 0;
   totaloutputbuffercount[i] = 0;
   totalinputsamplecount[i] = -1;  // so that first increment goes to zero
   }

 while(1==1) {  // repeating loop

   if(stopft8)
	 {
     puts("UDP thread end; close sock4");
     ft8active = 0;
     close(sock4);
	 pthread_exit(NULL);
	 }

 // if(ret > 0){
  // if (FD_ISSET(sock4, &readfd)){
  //  printf("try read\n");
    count = recvfrom(sock4, &iqbuffer2, sizeof(iqbuffer2),0, (struct sockaddr*)&flex_addr, &addr_len);

    streamNo = (int16_t)iqbuffer2.stream_ID[3];
 
// build ft8buffer header

   memcpy(ft8buffer_out[streamNo].VITA_hdr1, iqbuffer2.VITA_hdr1,sizeof(iqbuffer2.VITA_hdr1));
   ft8buffer_out[streamNo].stream_ID[0] = 0x46;    // F
   ft8buffer_out[streamNo].stream_ID[1] = 0x54;    // T
   ft8buffer_out[streamNo].stream_ID[2] = iqbuffer2.stream_ID[2]; // copy from input
   ft8buffer_out[streamNo].stream_ID[3] = iqbuffer2.stream_ID[3]; // copy from input
   ft8buffer_out[streamNo].VITA_packetsize = sizeof(ft8buffer_out[0]);
   ft8buffer_out[streamNo].time_stamp = (uint32_t)time(NULL);
   ft8buffer_out[streamNo].sample_count = totaloutputbuffercount[streamNo];

  for(int inputbuffercount =0; inputbuffercount < 512; inputbuffercount++)
   {
   totalinputsamplecount[streamNo]++;  // goes to zero on first buffer
   if((totalinputsamplecount[streamNo] % 12) != 0)  // crummy decimation; TODO: correct this
     continue;
   // inputbuffercount is multiple of 12 (or zero); save it
   
   ft8buffer_out[streamNo].theDataSample[outputbuffercount[streamNo]].I_val = iqbuffer2.flexDataSample[inputbuffercount].I_val;
   ft8buffer_out[streamNo].theDataSample[outputbuffercount[streamNo]].Q_val = iqbuffer2.flexDataSample[inputbuffercount].Q_val;

    outputbuffercount[streamNo]++;

    if(outputbuffercount[streamNo] >= 1024)  // have we filled the output buffer?
     {
      printf("FT8: try to send, streamNo = %i \n",streamNo);

      int sentBytes = sendto(sockft8out, (const struct dataBuf *)&ft8buffer_out[streamNo], sizeof(ft8buffer_out[0]), 0, 
	      (struct sockaddr*)&client_addr2, sizeof(client_addr2));
       outputbuffercount[streamNo] = 0;
       totaloutputbuffercount[streamNo] ++;
   //    printf("sent bytes %i\n",sentBytes);
      }

    }  // end of inputbuffercount loop (512 samples in the flex IQ packet)


    
 //   }  // after FD_ISSET
 //  }  // end of if statement on return code from socket setup (if ret)
  } // end of repeating loop

 }  // end of function

//////////////////////////////////////////////////////////////////////////////
void *sendFlexData(void * threadid){
// forward IQ data from flex to LH in VITA format, with minor mods

// TODO: add 32-bit time stamp & buffer count in right place for use by LH (DRF ddata handler)
  uint64_t theSampleCount = 0;
  fd_set readfd;
  int count;
 // ft8active = 1;
  printf("in Flex DATA thread; start to send data; init sock5\n");
  sock5 = socket(AF_INET, SOCK_DGRAM, 0);
  printf("after socket assign, sock5= %i\n",sock5);
  if(sock5 < 0) {
    printf("sock5 error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
    }
  int yes = 1;  // make socket re-usable
  if(setsockopt(sock5, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
   printf("sock5: Error setting sock option SO_REUSEADDR\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
   }

  printf("sock5 created\n");
  int addr_len = sizeof(struct sockaddr_in);
  memset((void*)&flex_addr, 0, addr_len);
  flex_addr.sin_family = AF_INET;
  flex_addr.sin_addr.s_addr = htons(INADDR_ANY);
  flex_addr.sin_port = htons(FLEXDATA_IN);
  printf("bind sock\n:");
  int ret = bind(sock5, (struct sockaddr*)&flex_addr, addr_len);
  if (ret < 0){
    printf("sock5 (Flex) bind error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
     }
  FD_ZERO(&readfd);
  FD_SET(sock5, &readfd);
  printf("in flex Data thread read from port %i\n",FLEXDATA_IN);
  client_addr.sin_port = htons(LH_DATA_IN_port[1]);
 ret = 1;
 while(1==1) {  // repeating loop

   if(stopDataColl)
	 {
     puts("UDP thread end");
     ft8active = 0;
     close(sock5);
	 pthread_exit(NULL);
	 }

  if(ret > 0){
   if (FD_ISSET(sock5, &readfd)){
  //  printf("try read\n");
    count = recvfrom(sock5, &iqbuffer2_in, sizeof(iqbuffer2_in),0, (struct sockaddr*)&flex_addr, &addr_len);
 //   printf("bytes received = %i\n",count);
 //   printf("VITA header= %x %x\n",iqbuffer2_in.VITA_hdr1[0],iqbuffer2_in.VITA_hdr1[1]);
 //   printf("stream ID= %x%x%x%x\n", iqbuffer2_in.stream_ID[0],iqbuffer2_in.stream_ID[1], iqbuffer2_in.stream_ID[2],iqbuffer2_in.stream_ID[3]);
    memcpy(iqbuffer2_out.VITA_hdr1, iqbuffer2_in.VITA_hdr1, sizeof(iqbuffer2_out.VITA_hdr1));

    iqbuffer2_out.VITA_packetsize = sizeof(iqbuffer2_out);
    iqbuffer2_out.stream_ID[0] = 0x52;   // put "RG" into stream ID
    iqbuffer2_out.stream_ID[1] = 0x47;
    iqbuffer2_out.stream_ID[2] = 1;   // number of embedded subchannels in buffer
    iqbuffer2_out.stream_ID[3] = iqbuffer2_in.stream_ID[3];
    iqbuffer2_out.time_stamp = (uint32_t)time(NULL);
    iqbuffer2_out.sample_count = theSampleCount;
    theSampleCount++; // this is actually a packet count, at least for now
  //  printf("timestamp = %i \n",iqbuffer2_out.time_stamp);
    for(int i=0; i < 512; i++)
     {
      iqbuffer2_out.theDataSample[i] = iqbuffer2_in.theDataSample[i];
     }

    count = recvfrom(sock5, &iqbuffer2_in, sizeof(iqbuffer2_in),0, (struct sockaddr*)&flex_addr, &addr_len);
 //   printf("bytes received = %i\n",count);

    for(int i=0; i < 512; i++)
     {
      iqbuffer2_out.theDataSample[i + 512] = iqbuffer2_in.theDataSample[i];
     }

    int sentBytes = sendto(sock, (const struct dataBuf *)&iqbuffer2_out, sizeof(iqbuffer2_out), 0, 
	      (struct sockaddr*)&portF_addr, sizeof(portF_addr));
  //  printf("Send to portF, bytes = %i\n",sentBytes);

    }
   }  // end of repeating loop
  }

}

//////////// Listen for and process CC commands ///////////////////////////////////////
void *awaitCreateChannels(void *threadid) {

  char buffer[1024];
  int addr_len;
  int ret;
  int count;
  int optval;
  printf("Starting awaitCreateChannels thread, listening on Port %d (Port B)\n",CCport); 

  addr_len = sizeof(struct sockaddr_in);
  memset((void*)&server_addr, 0, addr_len);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htons(INADDR_ANY);

// set up for Port B reply
  server_addr.sin_port = htons(CCport);
 // server_addr.sin_port = CCport;
  printf("DEmain, bind sock1 to %s port  %i (%i)\n",inet_ntoa(server_addr.sin_addr),ntohs(server_addr.sin_port),CCport);
  ret = bind(sock1, (struct sockaddr*)&server_addr, addr_len);
  if (ret < 0) {
    perror("bind error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
    }

while(1)
   {
    printf("DE: Awaiting CC to come in on port B  %u\n", CCport);
    count = recvfrom(sock1, buffer, CCport , 0, (struct sockaddr*)&server_addr, &addr_len);
   // LH_port = ntohs(client_addr.sin_port);
    printf("command recd %c%c %x %x %x %x from port %d, bytes=%d\n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4],buffer[5], LH_port,count);

    if(strncmp(buffer, CREATE_CHANNEL ,2) == 0)
      {

      memcpy(d.mybuf1, buffer, sizeof(CONFIGBUF));

      printf("CREATE CHANNEL RECD, cmd = %s, channel#=%i port C = %hu, port F = %hu \n",
          d.myConfigBuf.cmd, d.myConfigBuf.channelNo, d.myConfigBuf.configPort, d.myConfigBuf.dataPort);

      LH_CONF_IN_port[d.myConfigBuf.channelNo] = d.myConfigBuf.configPort;  // Port C
      LH_DATA_IN_port[d.myConfigBuf.channelNo] = d.myConfigBuf.dataPort;  // Port F
      printf("ports set up; now set up sock\n");
      if (sock = socket(AF_INET, SOCK_DGRAM, 0) == -1)
          printf("creating sock failed\n");  // set up for sending to port F
      else
          printf("sock created\n");
      addr_len = sizeof(struct sockaddr_in);
      memset((void*)&portF_addr, 0, addr_len);
      portF_addr.sin_family = AF_INET;
   //   portF_addr.sin_addr.s_addr = htons(LH_IP);
      portF_addr.sin_addr.s_addr = client_addr.sin_addr.s_addr;
      portF_addr.sin_port = htons(LH_DATA_IN_port[d.myConfigBuf.channelNo]);
      printf("Bind to port %i\n",LH_DATA_IN_port[d.myConfigBuf.channelNo]);


      ret = bind(sock, (struct sockaddr*)&portF_addr, addr_len);
      printf("bind ret=%i\n",ret);
      // set this befoe we change the buffer to reflect DE port setup
      client_addr.sin_port = htons(LH_CONF_IN_port[d.myConfigBuf.channelNo]);   // the ACK ges back to Port C
      printf("Set up the AK\n");
      strncpy(d.myConfigBuf.cmd, "AK", 2);
     
      d.myConfigBuf.configPort = DE_CH_IN_port[ d.myConfigBuf.channelNo]; // this is port D for this channel
      d.myConfigBuf.dataPort = 0;  // this is Port E (mic) - currently unused
      printf("Sending AK to port %i\n",client_addr.sin_port );
      count = sendto(sock1, d.mybuf1, sizeof(d.myConfigBuf), 0, (struct sockaddr*)&client_addr, addr_len);
      printf("response AK %i %i, bytes = %d  sent to ", d.myConfigBuf.configPort, ntohs(d.myConfigBuf.dataPort) ,count);
      printf(" IP: %s, Port: %d\n", 
      inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

  //   cmdport = DE_CONF_IN_portB;  // all further commands come in here
    }
   }
  return 0;

}


//void *handleCommands(void *threadid, int channelNo)
void *handleCommands(void* c) 
 {
  int channelNo = (int)c;
  printf("Starting command processor for channel %i\n",channelNo);

  while (LH_CONF_IN_port[channelNo] == 0)
  {
   //printf("LH_CONF_IN_port[ %i ] = %i\n",channelNo, LH_CONF_IN_port[channelNo]);
   sleep(0.1);

  } ;

  printf("Starting channel %i command processing\n",channelNo);
  printf("Port = %i\n",LH_CONF_IN_port[channelNo] );
  struct commandBuf cmdBuf;

  int addr_len;
  int ret;
  int count;
  int optval;
  int sock2;
// create & bind socket for inbound config packets
  fd_set readcfg;
  sock2 = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  setsockopt(sock2, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
  if(sock2 < 0 ) { perror("sock2 error\n"); return (void *) -1; }
  addr_len = sizeof(struct sockaddr_in);
  memset((void*)&config_in_addr,0,addr_len);
  config_in_addr.sin_family = AF_INET;
  config_in_addr.sin_addr.s_addr = htons(INADDR_ANY);
  config_in_addr.sin_port = htons(DE_CH_IN_port[channelNo]);  // listen on Port D
  ret = bind(sock2,(struct sockaddr *)&config_in_addr,addr_len);
  if(ret < 0) { perror("sock2 bind error\n"); return (void *) -1; }
  
  while(1)
    {
    memset(&cmdBuf,0,sizeof(cmdBuf));  // clear this area
    printf("Ready to receive command on port D = %i\n", DE_CH_IN_port[channelNo]);
 //   count = recvfrom(sock2, cb.configBuffer, sizeof(cb.configBuffer) , 0, 
 //       (struct sockaddr*)&config_in_addr, &addr_len);
    count = recvfrom(sock2, &cmdBuf, sizeof(cmdBuf) , 0, 
        (struct sockaddr*)&config_in_addr, &addr_len);
    printf("** Command received, %i bytes, cmd=%s\n",count,cmdBuf.cmd);

    if(memcmp(cmdBuf.cmd, STATUS_INQUIRY, 2) == 0)
      {
      printf("STATUS INQUIRY received\n");
      client_addr.sin_port = htons(LH_CONF_IN_port[channelNo] ); 
      count = 0;
      count = sendto(sock1, "OK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
      printf("Response OK = %u bytes sent to LH port %u \n ",count, LH_CONF_IN_port[channelNo]) ;
      }

    // is this CH command?
    if(memcmp(cmdBuf.cmd, CONFIG_CHANNELS, 2) == 0)
      {
      printf(" **********  CONFIG CHANNELS received %s\n", cmdBuf.cmd);
      memcpy(&cb,&cmdBuf,count);
      int channelNo = cb.chBuf.channelNo;
      noOfChannels = cb.chBuf.activeSubChannels;
      dataRate = cb.chBuf.channelDatarate;
      printf("Active subchannels: %i, rate = %i\n", noOfChannels, dataRate);
      for (int i=0; i < noOfChannels; i++) 
        {
        if(cb.chBuf.channelDef[i].antennaPort == -1)  // means this channel is off
          continue;
        else
          printf("Subchannel %i, Port %i, Freq %lf\n", cb.chBuf.channelDef[i].subChannelNo, 
          cb.chBuf.channelDef[i].antennaPort, cb.chBuf.channelDef[i].channelFreq);
        }
  
      client_addr.sin_port = htons(LH_CONF_IN_port[channelNo] ); 
      count = 0;
      count = sendto(sock1, "AK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
      printf("Response AK = %u bytes sent to LH port %u \n ",count, LH_CONF_IN_port[channelNo]) ;
      } // end of handling CH

     if(memcmp(cmdBuf.cmd, DATARATE_INQUIRY, 2) == 0)
      {
      printf(" ******** DATARATE INQUIRY received \n");
      sleep(0.2);
      DATARATEBUF myDataRateBuf;
      memset(&myDataRateBuf,0,sizeof(myDataRateBuf));
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
      client_addr.sin_port = htons(LH_CONF_IN_port[channelNo] ); 
      printf("Sending Datarate response to LH\n");
      count = 0;
      count = sendto(sock1, &myDataRateBuf, sizeof(myDataRateBuf), 0, (struct sockaddr*)&client_addr, addr_len);
      printf("Response: DATARATEBUF of %u bytes sent to LH port %u \n ",count, LH_CONF_IN_port[channelNo]) ;

      }

    }
///////////////////// end of command processing section ////////////////





  

}


/////////// wait for & process CONFIG_CHANNELS (CH) /////////////////////////////
void *awaitConfig(void *threadid) {

  config_busy = 1;
  printf("Starting await-config thread listening on port %d (Port D)\n",DE_CH_IN_port[0]);
//  printf("... will send data to LH port %d\n",LH_DATA_IN_port);

  int addr_len;
  int ret;
  int count;
  int optval;
  int sock2;
// create & bind socket for inbound config packets
  fd_set readcfg;
  sock2 = socket(AF_INET,SOCK_DGRAM,0);
  setsockopt(sock2, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
  if(sock2 < 0 ) { perror("sock2 error\n"); return (void *) -1; }
  addr_len = sizeof(struct sockaddr_in);
  memset((void*)&config_in_addr,0,addr_len);
  config_in_addr.sin_family = AF_INET;
  config_in_addr.sin_addr.s_addr = htons(INADDR_ANY);
  config_in_addr.sin_port = htons(DE_CH_IN_port[0]);  // listen on Port D
  ret = bind(sock2,(struct sockaddr *)&config_in_addr,addr_len);
  if(ret < 0) { perror("sock2 bind error\n"); return (void *) -1; }
  
  while(1)
    {
    count = recvfrom(sock2, cb.configBuffer, sizeof(cb.configBuffer) , 0, 
        (struct sockaddr*)&config_in_addr, &addr_len);

    printf("CHANNEL Setup CH received %s\n",cb.chBuf.chCommand);
    int channelNo = cb.chBuf.channelNo;
    noOfChannels = cb.chBuf.activeSubChannels;
    dataRate = cb.chBuf.channelDatarate;
    printf("active channels: %i, rate = %i\n", noOfChannels, dataRate);
    for (int i=0; i < noOfChannels; i++) 
      {
      if(cb.chBuf.channelDef[i].antennaPort == -1)  // means this channel is off
        continue;
      else
        printf("%i, Channel %i, Port %i, Freq %lf\n", i, cb.chBuf.channelDef[i].subChannelNo, 
        cb.chBuf.channelDef[i].antennaPort, cb.chBuf.channelDef[i].channelFreq);
      }
  
    client_addr.sin_port = htons(LH_CONF_IN_port[cb.chBuf.channelNo] ); 

    count = sendto(sock, "AK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
    printf("response = %u bytes sent to LH port %u \n ",count, LH_CONF_IN_port[cb.chBuf.channelNo]) ;
    }
}

//////////////////////////////////////////////////////////////////////////////////
// this thread gets UDP packets being sent by pihpsdr, forwards to mainctl on command
void *sendData1(void *threadid) {

  int yes = 1;
 // struct sockaddr_in client_addr3;
  struct sockaddr_in server_addr3;
  int addr_len;
  int count;
  int ret;
  long bufcount = 0;
  int sentBytes = 0;
  fd_set readfd;
  //char buffer[9000];

  sock3 = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock3 < 0) {
    perror("sock3 error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
  }

  addr_len = sizeof(struct sockaddr_in);

  memset((void*)&server_addr3, 0, addr_len);
  server_addr3.sin_family = AF_INET;
  server_addr3.sin_addr.s_addr = htons(INADDR_ANY);
  server_addr3.sin_port = htons(UDPPORT);

  ret = bind(sock3, (struct sockaddr*)&server_addr3, addr_len);
  if (ret < 0) {
    perror("UDP bind error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
  }
  while (1) {
    FD_ZERO(&readfd);
    FD_SET(sock3, &readfd);
    printf("read from port %i\n",UDPPORT);
  //  ret = select(sock3, &readfd, NULL, NULL, 0);
    ret = 1;
  //  printf("ret = %i\n",ret);
    if (ret > 0) {
      if (FD_ISSET(sock3, &readfd)) {
        printf("attempt read\n");
        count = recvfrom(sock3, &iqbuffer, sizeof(iqbuffer), 0, (struct sockaddr*)&server_addr3, &addr_len);
   //     count = recvfrom(sock3, &iqbuffer2, sizeof(iqbuffer2), 0, (struct sockaddr*)&server_addr3, &addr_len);
        printf("bytes received:  %i\n",count);


	    time_t epoch = time(NULL);
	//printf("unix time = %ld\n", epoch);
    //    strncpy(iqbuffer.bufType,"RG",2);
	    iqbuffer.timeStamp = (double) epoch;
        iqbuffer.channelCount = 1;  // probably duplicates functinoality in pihpsdr
   //     iqbuffer.dval.bufCount = bufcount++;
        iqbuffer.channelNo = 0;
        client_addr.sin_port = htons(LH_DATA_IN_port[0]);

        printf("forwarding bytes %i, count=%li\n",count,bufcount);
        sentBytes = sendto(sock, (const struct dataBuf *)&iqbuffer, sizeof(iqbuffer), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        printf("port %i, bytes sent: %i \n",LH_DATA_IN_port[0], sentBytes); 

        if(stopData)
	    {
         puts("UDP thread end; close socket");
         close(sock3);
	     pthread_exit(NULL);
	    }

      }
    }

  }

}



/////////////////////////////////////////////////////////////////////////////////////
///// Data acquisition (ring buffer or firehose) simulation thread ////////////////////////
//////////  creates a totally simulated signal (no data from any radio )/////////////////
void *sendData(void *threadid) {
  int addr_len;
  
  //memset((void*)&server_addr, 0, addr_len);
  printf("starting thread, to send to LH port %d, using following %i channels:\n",LH_DATA_IN_port[0], noOfChannels);

    for (int i=0; i < noOfChannels; i++) 
      {
      if(cb.chBuf.channelDef[i].antennaPort == -1)  // means this channel is off
        continue;
      else
        printf("%i, Channel %i, Port %i, Freq %lf\n", i, cb.chBuf.channelDef[i].subChannelNo, 
        cb.chBuf.channelDef[i].antennaPort, cb.chBuf.channelDef[i].channelFreq);
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
    printf("Sample count per buffer: %i \n", sampleCount);
    printf("Data rate: %i sps \n",dataRate);
    double bufferDelay = ((double)sampleCount / (double)dataRate) / 1E-06;
    printf("Delay per buf, microsec = %.3E \n",bufferDelay);

    double theStep = 6.283185307179586476925286766559 * (double)noOfChannels / 1024.0;
    int k = 0;
    printf("start data loop\n");
	for (int i = 0; i < (sampleCount * noOfChannels); i=i+noOfChannels) {
     for(int j = 0; j < noOfChannels;j++) {

      // here the float j produces different frequency for each channel
	//  I =  sin ( (double)i * (float)(j+1) * 3.1415926535897932384626433832795 / (double)(sampleCount));
	//  Q =  cos ( (double)i * (float)(j+1) * 3.1415926535897932384626433832795 / (double)(sampleCount));

// simple, single frequency
     I = cos ((double)k * theStep * (double)(j+1)  );  
     Q = sin ((double)k * theStep * (double)(j+1)  );  
  

      I = 0.7 * cos ((double)k * theStep * (double)(j+1) ) + 0.3 * cos((double)k * 3.0 * theStep * (double)(j+1) );  // the real part
      Q = 0.7 * sin ((double)k * theStep * (double)(j+1) ) + 0.3 * sin((double)k * 3.0 * theStep * (double)(j+1) );  // the imaginary part

      if(i<10) printf("%i  %i   %i    %f    %f   \n",i,j,k,I,Q);
	  myBuffer.theDataSample[i+j].I_val = I;
	  myBuffer.theDataSample[i+j].Q_val = Q;
           }
      k++;
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
   
    client_addr.sin_port = htons(LH_DATA_IN_port[0]);
    sentBytes = sendto(sock, (const struct dataBuf *)&myBuffer, sizeof(myBuffer), 0, 
	   (struct sockaddr*)&client_addr, sizeof(client_addr));

 //   fprintf(stderr,"UDP message sent from thread to port %u. bytes= %ld\n", 
   //        LH_DATA_IN_port, sentBytes); 
   // sleep(1);
    usleep(bufferDelay);  // wait for this many microseconds
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

/////////////////////////////////////////////////////////////////////
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
int *run_DE(void) 
  {
  stoplink = 0;
  stopData = 0;
  stopft8 = 0;
  ft8active = 0;
  config_busy = 0;
  int addr_len;
  int count;
  int ret;
  //DE_CONF_IN_portD = 50001;  //fixed port on which to receive config request (CC)


  fd_set readfd;
  char buffer[1024];

  sock = socket(AF_INET, SOCK_DGRAM, 0);  // for sending flex spectrum data out
  if (sock < 0) {
    perror("sock error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
    }

  sock1 = socket(AF_INET, SOCK_DGRAM, 0);  // for reply via Port B
  if (sock1 < 0) {
    perror("sock1 error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
    }

  sock4 = socket(AF_INET, SOCK_DGRAM, 0);  // for receiving IQ packets from FlexRadio (FT8)
  if (sock4 < 0) {
    perror("sock4 error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
    }

  sock5 = socket(AF_INET, SOCK_DGRAM, 0);  // for receiving IQ packets from FlexRadio (RG)
  if (sock5 < 0) {
    perror("sock5 error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
    }

  sockft8out = socket(AF_INET, SOCK_DGRAM, 0);  // for sending ft8 data out
  if (sockft8out < 0) {
    perror("sockft8out error\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
    }


  pthread_t thread3;
  printf("Starting awaitConfigChannels\n");
  int rc = pthread_create(&thread3, NULL, awaitCreateChannels, NULL);
  printf("retcode after thread create = %i\n",rc);

  printf("awaitCC started; startng command processing thread\n");
  pthread_t thread4[3];
  int c0 = 0;
  rc = pthread_create(&thread4[0], 0, handleCommands, (void*)c0);

  int c = 1;
  rc = pthread_create(&thread4[1], 0, handleCommands, (void*)c);
  
  rc = pthread_join(thread3, NULL);
  printf("record after thread join =%i\n",rc);

  puts("Set up sock6 socket");


  if((sock6 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
     { perror("sock6 create error\n"); return (void *) -1; }


  int yes = 1;  // make socket re-usable
  if(setsockopt(sock6, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
   printf("sock6: Error setting sock option SO_REUSEADDR\n");
    int r=-1;
    int *ptoi;
    ptoi = &r;
    return (ptoi);
   }


  addr_len = sizeof(struct sockaddr_in);
  memset((void*)&client_addr,0,addr_len);
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htons(INADDR_ANY);
  client_addr.sin_port = htons(DE_CH_IN_port[0]);  // listen on Port D
 // client_addr.sin_port = htons("50001");  // listen on Port D
  ret = bind(sock6,(struct sockaddr *)&client_addr,addr_len);
  if(ret < 0) { perror("sock1 bind error\n"); return (void *) -1; }


  puts("Now starting command processing loop");

  /////////////////////////////// control loop ////////////////////
  while(1)
    { 
    printf("Awaiting command to come in on port %u\n", DE_CH_IN_port[0]);


    struct commandBuf cmdBuf;
    count = recvfrom(sock6, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len); 
   // LH_port = ntohs(client_addr.sin_port);
    printf("command recd %c%c %x %x %x %x from port %d, bytes=%d\n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4],buffer[5], DE_CH_IN_port[0],count);
    char bufstr[100];
    memcpy(bufstr, buffer, count);
    memcpy(&cmdBuf, buffer, count);
    printf("Raw buf= %s\n",bufstr);
    printf("cmdBuf = %s   %i\n",cmdBuf.cmd, cmdBuf.channelNo);
    int channelNo = cmdBuf.channelNo;
   // command processsing
    printf("check for S? \n");
    if(strncmp(bufstr, "S?",2) == 0 )
	  { 
	  printf("STATUS INQUIRY\n");
  //    client_addr.sin_port = htons(LH_port);  // this may wipe desired port
      client_addr.sin_port = LH_port;  // this works for status but ruins data collection
      count = sendto(sock1, "OK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
      printf("response = %d  sent to ",count);
      printf(" IP: %s, Port: %d\n", 
      inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	  continue;
	  }


//  ??? there is a thread for this - probably unnecessary
    printf("check for CH \n");
    if(memcmp(bufstr, CONFIG_CHANNELS,2) == 0)
      {
      memcpy(cb.configBuffer,bufstr,sizeof(cb.configBuffer));
      printf("CHANNEL Setup CH received %s\n",cb.chBuf.chCommand);
      for (int i=0;i < 14; i++)
         printf("%x ",cb.configBuffer[i]);
      
      printf("\n");
      noOfChannels = cb.chBuf.activeSubChannels;
      dataRate = cb.chBuf.channelDatarate;
      int channelNo = cb.chBuf.channelNo;
      printf("VITA format requested = %s\n",cb.chBuf.VITA_type);
      printf("active channels: %i, rate = %i\n", noOfChannels, dataRate);
      for (int i=0; i < noOfChannels; i++) 
        {
        if(cb.chBuf.channelDef[i].antennaPort == -1)  // means this channel is off
          continue;
        else
          printf("%i, Channel %i, Port %i, Freq %lf\n", i, cb.chBuf.channelDef[i].subChannelNo, 
          cb.chBuf.channelDef[i].antennaPort, cb.chBuf.channelDef[i].channelFreq);
         }
  
      client_addr.sin_port = htons(LH_CONF_IN_port[channelNo] ); 

      count = sendto(sock, "AK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
      printf("response = %u bytes sent to LH port %u \n ",count, LH_CONF_IN_port[channelNo]) ;
      continue;
      }


printf("check for R? \n");
printf("value = %c%c\n",bufstr[0],bufstr[1]);
printf("memcmp = %i\n",memcmp(bufstr,"R?",2));
    if(memcmp(bufstr, "R?",2) == 0)   // Request for list of data rates
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
     client_addr.sin_port = htons(LH_CONF_IN_port[channelNo]);

      count = sendto(sock1, (const struct datarateBuf *)&myDataRateBuf, sizeof(DATARATEBUF), 0, (struct sockaddr*)&client_addr, addr_len);
      printf("response = %d  sent to ",count);
      printf(" IP: %s, Port: %d\n", 
      inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	  continue;
     }


printf("check for UL \n");
    if(memcmp(bufstr, "UL",2) == 0)
	  {  // future function for allowing LH to drop its link to this DE
	  printf("stoplink\n");
	  stoplink = 1;
	  continue;
	  }

printf("check for XC \n");
    if(memcmp(bufstr, "XC",2)==0)
	  {
	  printf("Main loop stopping data acquisition\n");
	  stopDataColl = 1;
	  continue;
	  }


printf("check for SF \n");
printf("I think it is: %c %c \n",bufstr[0],bufstr[1]);
//       for getting flex FT 8 data
    if(memcmp(bufstr, "SF",2)==0)  // Flex data
     {
      printf("Start FlexRadio / FT8 command received; starting thread\n");
      stopft8 = 0;
      pthread_t thread1;
      int rc = pthread_create(&thread1, NULL, sendFT8flex, NULL);
      continue;
     }




printf("check for XF \n");
    if(memcmp(bufstr, "XF",2)==0)
      {
      printf("Stop FT8 command received\n");
      stopft8 = 1;
      continue;
      }
printf("check for XX \n");
    if(memcmp(bufstr, "XX",2)==0)
	  {
	  printf("HALTING\n");
	  return 0;
	  }
printf("check for SC \n");
    if(memcmp(bufstr, "SC",2)==0)
	  {
	  puts("starting sendData");
	  stopDataColl = 0;
  	  long j = 1;
  	  pthread_t datathread;
 // 	  int rc = pthread_create(&datathread, NULL, sendData1, (void *)j);
  	  int rc = pthread_create(&datathread, NULL, sendFlexData, (void *)j);
  	  printf("thread start rc = %d\n",rc);

// following code is superfluous; DE does not plan to ACK the SC command

	  printf("SEND ACK to port %u\n",LH_CONF_IN_port[channelNo]);
      client_addr.sin_port = htons(LH_CONF_IN_port[channelNo]);  // this may wipe desired port
      count = sendto(sock1, "AK", 2, 0, (struct sockaddr*)&client_addr, addr_len);
      continue;
	  }

  // in case we are running but get another discovery packet
  // This essentially switches DE simulator to talk to a different LH and/or port.
  // TODO: Need to keep track of multiple LH devices, allow link & unlink
printf("check for discovery packet \n");
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

  //} // end of discovery loop
}

int main(int argc, char** argv) {
 //while(1)
 {
// arguments are:
//  1 - port B
//  2 - IP addr of LH
//  3 - port A
 printf("- - - - - Starting DEmain; port B = %s\n",argv[1]);

// port on which to await CC   (create channel request)
 CCport = atoi(argv[1]);  // port B
 LH_port = ntohs(atoi(argv[3]));
 inet_pton(AF_INET, argv[2], &client_addr.sin_addr);  // LH IP addr
 inet_pton(AF_INET, argv[2], &LH_IP);  // this is not used TODO; fix
 client_addr.sin_port = LH_port;  // port A
 
 int *r =  run_DE();
 printf("DE exited, rc = %i; closing sockets & threads\n", *r);
 close(sock);
 close(sock1);
 //close(sock2);
 stoplink = 1;
 stopData = 1;
 stopft8 = 1;
 printf("attempting restart\n");
 }

}


