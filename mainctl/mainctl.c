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
* External packages:
  hdf5
  Digital_RF
  sshpass
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <json.h>
#include <pthread.h>
#include <time.h>
#include <complex.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include "CUnit/CUnit.h"
#include "CUnit/Basic.h"
#include <assert.h>
#include "discovered.h"
#include "de_signals.h"

extern DISCOVERED UDPdiscover();
static uint16_t LH_port;   // port A, used on LH (local host) for sending to DE, and will listen on
static uint16_t DE_port;   // port B, that DE will listen on
static char DE_IP[16];

// These ports work in sets, assigned to an application use case.
// The subscript is the channel#.
// 0 = RG data,  1 = FT8,  2 = WSPR
static uint16_t LH_CONF_IN_port[3];  // port C, receives ACK or NAK from config request
static uint16_t DE_CONF_IN_port[3];  // port D; DE listens for config request on this port
static uint16_t LH_DATA_IN_port[3];  // port F; LH listens for spectrum data on this port
static uint16_t DE_DATA_IN_port[3];  // port E; DE listens for xmit data on this port

union {
      char mybuf1[100];
      CONFIGBUF myConfigBuf;
          } d;
union {
      char mybuf2[sizeof(CHANNELBUF)];
      CHANNELBUF channelBuffer;
          } h;

CONFIGBUF configRequest;   

#define DEVICE_TANGERINE 7  // TangerineSDR for now



static  char* sysCommand1;
static  char* sysCommand2;
static	uint64_t vector_length = 1024; /* number of samples written for each call -  */
static  uint64_t vector_sum = 0;

static long buffers_received = 0;  // for counting UDP buffers rec'd in case any dropped in transport

static char ringbuffer_path[100];
static char total_hdf5_path[100];
static char firehoseR_path[100];
static char hdf5subdirectory[16];
static long packetCount;
static int recv_port_status = 0;
static int recv_port_status_ft8 = 0;

struct sockaddr_in send_addr;         // used for sending commands to DE
struct sockaddr_in recv_addr;         // for command replies from DE (ACK, NAK)
struct sockaddr_in send_config_addr;  // used for sending config req (CH) to DE
struct sockaddr_in recv_config_addr;  // for config replies from DE
struct sockaddr_in recv_data_addr;    // for data coming from DE


const char *configPath;
static int num_items;  // number of config items found
static char configresult[100];
static char target[30];
static int num_items = 0;
static int snapshotterMode = 0;
static int ringbufferMode = 0;
static int firehoseLMode = 0;
static int firehoseRMode = 0;
static int firehoseUploadActive = 0;
static char pathToRAMdisk[100];
static int uploadInProgress = 0;
// for communications to Central Host
static char central_host[100];
static uint16_t central_port;
// the configuration file
static config_t cfg;
static config_setting_t *setting;

static char data_path[100];
static char temp_path[100];
static char the_node[20];

static int dataRatesReceived = 0;
static int DE_OK = 0;


//// *********************** Start of Code  *******************************//////

/////////////////////////////////////////////////////////////////////////////
/////// function to read config items from the (python) config file /////////
int rconfig(char * arg, char * result, int testThis) {
const char delimiters[] = " =";
printf("start fcn looking for %s\n", arg);
FILE *fp;
char *line = NULL;
size_t len = 0;
ssize_t read;
char *token, *cp;
if (testThis)
  {
  fp = fopen( "/home/odroid/projects/TangerineSDR-notes/flask/config.ini", "r");
  }
else
  fp = fopen(configPath, "r");
if (fp == NULL)
  {
  printf("ERROR - could not open config file at %s\n",configPath);
  printf("ABEND 102");
  exit(-1);
  }
//puts("read config");
while ((read = getline(&line, &len, fp)) != -1) {
 // printf("line length %zu: ",read);
 // printf("%s \n",line);
  cp = strdup(line);  // allocate enuff memory for a copy of this
  //printf("cp=%s\n",cp);
  token = strtok(cp, delimiters);
 // printf("first token='%s'\n",token);
  if(strcmp(arg,token) == 0)
   {
  token = strtok(NULL, delimiters);
 // printf("second token=%s\n",token);
 // printf("config value found = '%s', length = %lu\n",token,strlen(token));
  strncpy(result,token,strlen(token)-1);
  result[strlen(token)-1] = 0x00;  // terminate the string
  free(cp);
  fclose(fp);
  return(1);
   }
  }
  free(cp);
  fclose(fp);
  return(0);
}




///////////////////////////////////////////////////////////////////////////
///////// Thread for uploading firehoseR data to Central Control //////////
void firehose_uploader(void *threadid) {

  char sys_command[200];
  printf("firehoseR uploader thread starting\n");
  sleep(20);
  while(1)
   {
   if (firehoseUploadActive == 0)  // firehoseR upload halted
     {
     printf("------ FIREHOSE UPLOAD SHUTTING DOWN -------\n");
     return;
     }
   printf("------FIREHOSE UPLOAD-----------\n");

  sprintf(sys_command,"./firehose_xfer_auto.sh %s %s %s", data_path,temp_path,the_node);
  printf("M: Uploader - executing command: %s \n",sys_command); 
  int r = system(sys_command); 
  printf("M: System command retcode=%i\n",r);

   sleep(10);

   }

  return;
}



//////////////////////////////////////////////////////////////////////////
//////////   Function to send command to DE //////////////////////////////
void sendCommandToDE(int channelNo, char command[2]) {

//  if(channelNo == 1)
  //  LH_CONF_IN_port[1] = 40003;  // temporary  TODO: fix

  // TODO: temporary until we find out what sets the port to zero for channel 1
  char portname[15];
  sprintf(portname,"configport%i",channelNo);
  printf("M: portname= %s\n",portname);
  num_items = rconfig(portname,configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - configport setting not found in config.ini\n");
    }
  else
    {
    printf("M: configport CONFIG RESULT = '%s'\n",configresult);
    LH_CONF_IN_port[channelNo] = atoi(configresult);
    } 
  printf("M: Port C now set to %i\n", LH_CONF_IN_port[channelNo] ); 



  printf("M: Prep to send command %s to DE, channel %i\n",command,channelNo);
  printf("M: Now, LH_CONF_IN_port[%i] = %i\n",channelNo,LH_CONF_IN_port[channelNo]);
  struct commandBuf cmdBuf;
  char buf[1024];
  memset(&cmdBuf, 0, sizeof(cmdBuf));
  memcpy(&cmdBuf,command, 2);

  // send CH command to Port D
    struct sockaddr_in si_DE;
    struct sockaddr_in si_LH;
    struct sockaddr_in si_LH2;

    int s, s1 ;
    printf("M: define socket\n");
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	  {
        perror("socket");
	  	printf("socket s error");
	  }  
    if ( (s1=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	  {
        perror("socket");
	  	printf("socket s1 error");
	  } 

    socklen_t addr_size;
    memset((char *) &si_DE, 0, sizeof(si_DE));
    memset((char *) &si_LH, 0, sizeof(si_LH));
    memset((char *) &si_LH2, 0, sizeof(si_LH2));
    si_DE.sin_family = AF_INET;
    si_LH.sin_family = AF_INET;
    si_DE.sin_port = htons(DE_CONF_IN_port[channelNo]);
    si_LH.sin_port = htons(LH_CONF_IN_port[channelNo]);

    struct commandBuf reqBuf;
    struct datarateBuf replyBuf;
    memset((char *)&reqBuf,0,sizeof(reqBuf));
    if (inet_aton(DE_IP , &si_DE.sin_addr) == 0) 
	  {
	  	fprintf(stderr, "inet_aton() failed\n");
	  }
    si_LH.sin_addr.s_addr = htonl(INADDR_ANY);

 // Prep to receive AK from DE
    int yes = 1;
    if(setsockopt(s1, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
      perror("sockopt");
      }

    printf("M: Bind to port %i for receiving a response from DE\n",LH_CONF_IN_port[channelNo]);
    if( bind(s1, (struct sockaddr*)&si_LH, sizeof(si_LH)) < 0)
      {
        perror("socket");
        printf("s1 bind error\n");
      }

    printf("M: sending %s\n",cmdBuf.cmd);
    socklen_t sil = sizeof(si_DE);
		//send the message
 //   if (sendto(s, &cmdBuf, sizeof(cmdBuf) , 0 , (struct sockaddr *) &si_DE, sizeof(si_DE)) <0 )
    if (sendto(s, &cmdBuf, 4 , 0 , (struct sockaddr *) &si_DE, sil) <0 )
		{
            perror("sendto");
			printf("M: sendto() error\n");
         //   die("send to DE error");
		}

    ssize_t recv_len = 0;
    printf("M: waiting for AK or other DE response, on port %i\n",ntohs(si_LH.sin_port));
    int addr_len = sizeof(struct sockaddr_in);

    memset(&buf,0,sizeof(buf));

    printf("M: now: recvfrom (coming from DE)\n");

// Unexplained result here - the DE returns AK, and it is in the buffer, but
//  recv_len is set to zero. It should return buffer length.
// Zero may indicate the socket is closed (maybe by the other system?)


    if (recv_len = recvfrom(s1, buf, 2, 0, (struct sockaddr *) &si_LH, 
               &addr_len) < 0)
        {  
           perror("recvfrom");
           printf("recvfrom error\n");
         }
    else
         {
         printf("M: recvfrom returned retcode %li\n",recv_len);
         if(recv_len <= 0 )
           {
           printf("M: no length info received\n");
           }
         char test[3] = "";
         memcpy(&test,&buf,2);
         printf("M: Answer reeceived; Buffer starts with %s\n",test);

         if(memcmp(&buf, STATUS_OK ,2) == 0)
            {
            printf("Received OK from DE\n");
            DE_OK = 1;
            } 

         if(memcmp(&buf,DATARATE_RESPONSE,2) == 0)
           {
           printf("M: DR detected\n");
           memcpy(&replyBuf, buf, sizeof(replyBuf));
           }
         printf("M: Received %li bytes starting with %s from DE at %s:%d\n",recv_len,replyBuf.buftype,
            inet_ntoa(si_DE.sin_addr), ntohs(si_DE.sin_port));
         if (memcmp(replyBuf.buftype, DATARATE_RESPONSE, 2) == 0)
           {
           printf("M: Datarate response received\n");
           for(int i=0;i < 20;i++) {
             if(replyBuf.dataRate[i].rateNumber == 0)
               {
               close(s);
               close(s1);
               break;
               }
             printf("Rate # %i = %i sps\n",replyBuf.dataRate[i].rateNumber,
               replyBuf.dataRate[i].rateValue); 
             dataRatesReceived = i;
             }
         //  dataRatesReceived = 1;
           }
         }



    close(s);
    close(s1);
    puts("M: Sending of command to DE completed");
  // end of code to send command to Port D


}



/////////////////////////////////////////////////////////////////
//////////////  Thread for processing commands from app.py //////
void *  processUserActions(void *threadid){
  printf("Start processUserActions thread\n");
 // printf("Now, LH_CONF_IN_port[0] = %i\n",LH_CONF_IN_port[0]);

    int sockfd, connfd, len; 
    struct sockaddr_in servaddr, cli; 
  
    // socket create and verification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully created..\n"); 
    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(6100);  // TODO; use configured value
  int yes = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
         perror("setsockopt");
    }
    
  
    // Binding newly created socket to given IP and verification 
    if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) { 
        perror("bind");
        printf("socket bind failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully bound..\n"); 
  
    // Now server is ready to listen and verification 
    if ((listen(sockfd, 5)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } 
    else
        printf("Server listening..\n"); 
    len = sizeof(cli); 
  

    // Accept the data packet from client and verification 
    printf("Waiting to accept connection...\n");
    connfd = accept(sockfd, (struct sockaddr*)&cli, &len); 
    if (connfd < 0) { 
        printf("server acccept failed...\n"); 
        exit(0); 
    } 
    else
        printf("M: server acccepted the client...\n"); 
  
    struct commandBuf cmdBuf;
    ssize_t n; 

    while(1)
      {
        memset(&cmdBuf, 0, sizeof(cmdBuf));
        printf("M: Ready to process command from app.py\n");
        // read the message from client and copy it in buffer 
        n = read(connfd, &cmdBuf, sizeof(cmdBuf)); 
        // print buffer which contains the client contents 
        printf("M: Received %li bytes from client starting with %x %x\n", n, cmdBuf.cmd[0], cmdBuf.cmd[1]); 



        if (memcmp (cmdBuf.cmd, START_DATA_COLL, 2) == 0)
          {
          cmdBuf.channelNo = 0;  // this is for ringbuffer-type data
          // make sure this process is not inadvertently running
          char stoprgrcvr[50] = "killall -9 rgrcvr";
          printf("M: Issue command: %s\n",stoprgrcvr);
          int rcode = system(stoprgrcvr);
          char startrgrcvr[50] = "./rgrcvr &";
          printf("M: Issue command: %s\n",startrgrcvr);
          rcode = system(startrgrcvr);
          }
        if (memcmp (cmdBuf.cmd, STOP_DATA_COLL, 2) == 0)
          {
          // stop rgrcvr
          char stoprgrcvr[50] = "killall -9 rgrcvr";
          printf("M: Issue command: %s\n",stoprgrcvr);
          int rcode = system(stoprgrcvr);
          cmdBuf.channelNo = 0;  // this is for ringbuffer-type data
          // we set channelNo and pass this command on to DE
          }



        if (memcmp (cmdBuf.cmd, START_FT8_COLL, 2) == 0)  // SF
          {
          memcpy(&cmdBuf.cmd, "SC",2);
          cmdBuf.channelNo = 1;  // this is for FT8-type data
          }
        if (memcmp (cmdBuf.cmd, STOP_FT8_COLL, 2) == 0)
          {
          memcpy(&cmdBuf.cmd, "XC",2);
          cmdBuf.channelNo = 1;  // this is for FT8-type data
          }


        if (memcmp (cmdBuf.cmd, START_WSPR_COLL, 2) == 0)
          {
          cmdBuf.channelNo = 2;  // this is for WSPR-type data
          }

/*
        if(n<4)
          {
          cmdBuf.channelNo = 0; // for testing with nc, zero out channelNo
          }
        printf("Command is: %s for channel# %i\n",cmdBuf.cmd,cmdBuf.channelNo);
*/




        char commandToSend[2];

        printf("M: Forwarding command %s to DE, for channel %i\n", cmdBuf.cmd, cmdBuf.channelNo);
        sendCommandToDE(cmdBuf.channelNo, cmdBuf.cmd);


        printf("M: send internal AK to app.py\n");
        memcpy(cmdBuf.cmd,"AK",2);
        int wrc = write(connfd, &cmdBuf, 2);
        printf("M: rc = %i\n",wrc);

        if(n <= 0) 
         {
         sleep(1);
         continue;
          }

 
     }
    close(sockfd); 



}


///////////////// create file name for upload data //////////////////
const char * buildFileName(char * node, char * grid){

 static char thefilename[100] = "";
 time_t t= time(NULL);
 struct tm *tm = localtime(&t);
 char s[64];
 assert(strftime(s, sizeof(s), "%FT%H%M%SZ", tm));
 printf("computed time=%s\n",s);
 strcpy(thefilename, s);
 strcat(thefilename, "_");
 printf("the node = %s\n", node);
 strcat(thefilename, node);
 strcat(thefilename, "_T1_");
 strcat(thefilename, grid);
 strcat(thefilename, "_DRF.tar");
 printf("filename: %s\n", thefilename);
 return (char *)thefilename;

}

////// Function to prepare data in ring buffer for upload  /////
///// Triggered as part of responding to "DR" data request coming from Central Control
int prep_data_files(char *startDT, char *endDT, char *ringbuffer_path)
 {
// first: build list of data files for uplad using DRF "ls" utility
// build a drf ls command to find the files within time frame
  char fcommand[150];
 // char fcommand1[150];
  char fn[100] = "";
  char theNode[10] = "";
  char theGrid[10] = "";
  printf("ringbuffer_path=%s\n",ringbuffer_path);
// store the list of file names in the RAMdisk
  sprintf(fcommand,"drf ls %s -r -s %s -e %s > %s/dataFileList", ringbuffer_path, startDT, endDT, pathToRAMdisk);
  printf(" ** drf fcommand1='%s'\n",fcommand);
  int retcode = system(fcommand);  // execute the command
  printf("drf return code=%i\n",retcode);
// build arguments for a script that will tar the files, saving
// the compressed (tar) file in same location as the ringbuffer
  int num_items = rconfig("node",theNode,0);
  num_items = rconfig("grid",theGrid,0);
  strcpy(fn, (char *)buildFileName(theNode, theGrid));
  sprintf(fcommand,"./filecompress.sh %s  %s/dataFileList %s ", ringbuffer_path, pathToRAMdisk, fn);
  printf("** file compress command=%s\n",fcommand);
  retcode = retcode + system(fcommand); // execute the command
  printf("compress retcode = %i\n",retcode);
  return(retcode);
 }

/////////////////////////////////////////////////////////////////////////////////////
// the following function parses input to get start and end date-times for data upload
int getDataDates(char *input, char* startpoint, char* endpoint)
 {
 // printf("input to extract from= '%s'\n", input);
  int theLen = strlen(input);
  printf("len = %i\n",theLen);
  if(theLen < 180)
   {
   return(0);  // this is not a DR command
   }
  printf("last char = %c\n",input[theLen - 1]);
  int s = 0;
  int e = theLen;
  int j = 0;
  for(int i = theLen-20; i < theLen -1; i++)
   {
   endpoint[j] = input[i];
   j++;
   } 
  endpoint[j] = 0;
  j = 0;
  for(int i = theLen-40; i < theLen -21; i++)
   {
   startpoint[j] = input[i];
   j++;
   } 
  startpoint[j] = 0;
  return(1);
 }



////////////////////////////////////////////////////////////////////////////
/////////   Function to build channel config (CH) request & pass to DE  ////
int makeCHrequest(int channelNo){
  int myretcode = -1;
  printf("M: Build CH request\n");
  struct channelBuf chBuf;         // create & initialize channel description
  char b[sizeof(CHANNELBUF)];      // so we can copy to this later
  char target[20];

  memset(&chBuf, 0, sizeof(chBuf));
  memcpy(chBuf.chCommand, CONFIG_CHANNELS, 2);
  chBuf.channelNo = channelNo;

  // The 3 possible channel configuraations are slightly different, so we have 3
  // sections to build the right stuff to set up each in the DE.

  if(channelNo == 0)  // this is a RG-type channel
    {
    memcpy(chBuf.VITA_type,"VT",2);  // specify our special mod of VITA
    int channelCount = 0;
    // determine how many subchannels are configured
    num_items = rconfig("numchannels",configresult,0);
    if(num_items == 0)
      {
      printf("ERROR - numchannels setting not found in config.ini\n");
      }
    else
      {
      printf("numchannels CONFIG RESULT = '%s'\n",configresult);
      channelCount = atoi(configresult);
      printf("channel count= %i\n",channelCount);
      chBuf.activeSubChannels = channelCount;
      } 
    num_items = rconfig("datarate",configresult,0);
    if(num_items == 0)
      {
      printf("ERROR - datarate setting not found in config.ini\n");
      }
    else
      {
      printf("datarate CONFIG RESULT = '%s'\n",configresult);
      chBuf.channelDatarate = atoi(configresult);
      }
    for(int i = 0; i < channelCount;i++)
      {
      chBuf.channelDef[i].subChannelNo = i;

      sprintf(target,"p%i",i);  // fill in antenna setting
      num_items = rconfig(target,configresult,0);
      if(num_items == 0)
        {
        printf("ERROR - p%i setting not found in config.ini\n",i);
        }
      else
        {
        printf("p%i CONFIG RESULT = '%s'\n",i,configresult);
        chBuf.channelDef[i].antennaPort = atoi(configresult);
        }
      sprintf(target,"f%i",i);  // fill in subchannel center frequency
      num_items = rconfig(target,configresult,0);
      if(num_items == 0)
        {
        printf("ERROR - f%i setting not found in config.ini\n",i);
        }
      else
        {
        printf("f%i CONFIG RESULT = '%s'\n",i,configresult);
        chBuf.channelDef[i].channelFreq = (double)atof(configresult);
        }
      }  // end of subchannel for loop

    }  // end of handling channel zero

  if(channelNo == 1)  // this is a FT8-type channel
    {
    memcpy(chBuf.VITA_type,"V4",2);  // specify standard VITA-49
    int channelCount = 0;
    // determine how many subchannels are configured
    chBuf.channelDatarate = 4000;   // hard coded for FT8 decoder
    chBuf.activeSubChannels = 0;
    for(int i=0;i < 8;i++) {
      sprintf(target,"ftant%i",i);
      num_items = rconfig(target,configresult,0);
      if(num_items == 0)
        {
        printf("ERROR - ftant%i setting not found in config.ini\n",i);
        }
      else
        {
        printf("ftant%i CONFIG RESULT = '%s'\n",i,configresult);
        if(memcmp(configresult, "Off",1) == 0)  // if this channel turned off,
          continue;                             //  skip it 
        chBuf.activeSubChannels++;
        chBuf.channelDef[i].antennaPort = atoi(configresult);  // get antenna port
        chBuf.channelDef[i].subChannelNo = i;
        sprintf(target,"ft8%if",i);  // now look for the frequency of this subchannel
        num_items = rconfig(target,configresult,0);
        if(num_items == 0)
          {
          printf("ERROR - ft8%if setting not found in config.ini\n",i);
          }
        else
          {
          printf("ft8%if CONFIG RESULT = '%s'\n",i,configresult);
       // TODO: the following probably needs to be multiplied by 1,000,000.0
          chBuf.channelDef[i].channelFreq = (double) atof(configresult);
          }
        }
      }
    }  // end of handling channel 1



  // send CH command to Port D
    struct sockaddr_in si_DE;
    struct sockaddr_in si_LH;

    int s, s1;
    printf("define socket\n");
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	  {
	  	printf("socket s error");
	  }  
    if ( (s1=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	  {
	  	printf("socket s1 error");
	  } 
    memset((char *) &si_DE, 0, sizeof(si_DE));
    memset((char *) &si_LH, 0, sizeof(si_LH));
    si_DE.sin_family = AF_INET;
    si_LH.sin_family = AF_INET;
    si_DE.sin_port = htons(DE_CONF_IN_port[channelNo]);
    si_LH.sin_port = htons(LH_CONF_IN_port[channelNo]);
    struct commandBuf replyBuf;
    memset((char *)&replyBuf,0,sizeof(replyBuf));
    if (inet_aton(DE_IP , &si_DE.sin_addr) == 0) 
	  {
	  	fprintf(stderr, "inet_aton() failed\n");
	  }
    si_LH.sin_addr.s_addr = htonl(INADDR_ANY);
 // Prep to receive AK from DE
    printf("Bind to port for receiving AK or NK from DE\n");
    if( bind(s1, (struct sockaddr*)&si_LH, sizeof(si_LH)) == -1)
      {
        printf("s1 bind error\n");
     //   die("s1 bind error");
      }

    printf("send\n");
		//send the message
    if (sendto(s, &chBuf, sizeof(chBuf) , 0 , (struct sockaddr *) &si_DE, sizeof(si_DE))==-1)
		{
			printf("sendto() error\n");
         //   die("send to DE error");
		}
    int sDE = sizeof(si_DE);
    int recv_len = 0;
    if (recv_len = recvfrom(s1, &replyBuf, sizeof(replyBuf), 0, (struct sockaddr *) &si_DE, 
               &sDE ) == -1)
        {
           printf("recvfrom error\n");
        //   die("recvfrom DE error");

         }
    else
         {
         printf("Received %s from DE at %s:%d\n",replyBuf.cmd, inet_ntoa(si_DE.sin_addr),
                ntohs(si_DE.sin_port));

         if (memcmp(replyBuf.cmd,"AK",2) == 0)  // did we get AK from DE?
            myretcode = 0;
         }
 
    close(s);
    close(s1);
    return(myretcode);  // returns zero if expected "AK" received from DE (else -1)

  // end of code to send CH command to Port D

}


///////////////////// open config file ///////////////
int openConfigFile()
{
 // printf("test - config init\n");
  config_init(&cfg);

  /* Read the file. If there is an error, report it and exit. */

// The only thing we use this config file for is to get the path to the
// python config file. Seems like a kludge, but allows flexibility in
// system directory structure.
 // printf("test - read config file\n");
  if(! config_read_file(&cfg, "/home/odroid/projects/TangerineSDR-notes/mainctl/main.cfg"))
  {
    fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
            config_error_line(&cfg), config_error_text(&cfg));
    puts("ERROR - there is a problem with main.cfg configuration file");
    config_destroy(&cfg);
    return(EXIT_FAILURE);
  }
//  printf("test - look up config path\n");
  if(config_lookup_string(&cfg, "config_path", &configPath))
    printf("Setting config file path to: %s\n\n", configPath);
  else
    fprintf(stderr, "No 'config_path' setting in configuration file main.cfg.\n");
    return(EXIT_FAILURE);
 // printf("test - config path=%s\n",configPath);
  return(0);
} 

////////////// Data Uploader thread /////////////////////////////////////
void *dataUpload(void *threadid) {
  uploadInProgress = 1;
  char uploadCommand[300]="";
  char uploadPath[100]="";
  char logPath[100]="";
  char hostURL[80]="";
  char throttle[12]="";
  char node[10]="";
  char logEntry[100]="Upload requested,";
  printf("Upload thread starting\n");
  int rc = openConfigFile();
  printf("retcode from config file open=%i\n",rc);
  num_items = rconfig("central_host",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - Central Host setting not found in config.ini\n");
    }
  else
    {
    printf("central host CONFIG RESULT = '%s'\n",configresult);
    strcpy(hostURL,configresult);
    } 
  num_items = rconfig("upload_path",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - upload workarea path not found in config.ini\n");
    }
  else
    {
    printf("upload path CONFIG RESULT = '%s'\n",configresult);
    strcpy(uploadPath,configresult);
    }
  num_items = rconfig("log_path",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - upload log path not found in config.ini\n");
    }
  else
    {
    printf("upload log path CONFIG RESULT = '%s'\n",configresult);
    strcpy(logPath,configresult);
    }
  num_items = rconfig("throttle",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - throttle setting not found in config.ini\n");
    }
  else
    {
    printf("throttle CONFIG RESULT = '%s'\n",configresult);
    strcpy(throttle,configresult);
    }
  num_items = rconfig("node",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - node setting not found in config.ini\n");
    }
  else
    {
    printf("node CONFIG RESULT = '%s'\n",configresult);
    strcpy(node,configresult);
    }

  rc = system(logEntry);  // tell log what this is
  sprintf(logEntry,"date -u >>%s/upload.log",logPath);  // log time & date of upload attempt
  printf("log update: %s\n",logEntry);
  rc = system(logEntry);
  
// TODO: password for account needs to be included correctly (may use token)
  sprintf(uploadCommand,"lftp -e 'set net:limit-rate %s;mirror -R --Remove-source-files --verbose %s %s/sftp-test;exit' -u %s,%s sftp://%s >> %s/upload.log",throttle,uploadPath,node,node,"odroid",hostURL,logPath);
  printf("Upload command=%s\n",uploadCommand);
  rc = system(uploadCommand);  // just do it

}

///////////////////////// Discover HIPSDR compliant devices ///////////////
void discover_DE(){

  puts("UDPdiscovery    ******    *****");

  //DEdevice = UDPdiscoverNew();  // call UDP discovery rtn

// here we look for compatible device
  LH_port = 0;
  discovered[0]= UDPdiscover(&LH_port);    // pass handle to outbound port
  // discovery randomly selects an outbound port. DE will talk to that port.
  // Here we find out what that port is, so we can listen on it.
  // In documentation, this port is called "Port A"
  printf("discovery selected LH port %d (Port A)\n", LH_port); 
  puts("Here's what we found:");
  for(int i=0;i<MAX_DEVICES;i++)
	{
	 if(strlen(discovered[i].name) == 0 ) break;   // apparently the end of the list
     fprintf(stderr,"  ^^^^^^^ Device %d is %s at %s port %u  \n",i, discovered[i].name,
	   inet_ntoa(discovered[i].info.network.address.sin_addr),   
       htons(discovered[i].info.network.address.sin_port));
    // TODO: temporary way to pick device on the wire; 
    // will need to have a better way to select from multiple responding devices
	if(discovered[i].device == DEVICE_TANGERINE) 
	 {
		strcpy(DE_IP, inet_ntoa(discovered[i].info.network.address.sin_addr));
		DE_port = htons(discovered[i].info.network.address.sin_port);  // this is Port B
		printf("Selected Tangerine at port %u (Port B)\n",DE_port);
	  }
	}


}


///////////////// Error Reporting     ////////////////////////
void die(char *s)
{
	perror(s);
}

//////////////////////////////////////////////////////////////
////////////////////// Create Channel CC ///////////////////////
int create_channel(int channelNo) {
// This is a synchronous operation, as we cannot proceed until we have
// created channels; hence, we do not use the libuv asych calls.
  printf("M: Prep to send CREATE_CHANNEL for channel# %i\n",channelNo);
  // first we have to get the port numbers to bs used.

	struct sockaddr_in si_DE;
    struct sockaddr_in si_LH;
    struct configChannelRequest cc;
  // Build the CC command buffer
  memcpy(cc.cmd,CREATE_CHANNEL,2);
  cc.channelNo = channelNo;
  char portname[15];
  sprintf(portname,"configport%i",channelNo);
  printf("M: portname= %s\n",portname);
  num_items = rconfig(portname,configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - configport setting not found in config.ini\n");
    }
  else
    {
    printf("configport CONFIG RESULT = '%s'\n",configresult);
    cc.configPort = atoi(configresult);
    LH_CONF_IN_port[channelNo] = atoi(configresult);
    } 
  printf("Port C now set to %i\n",cc.configPort);  

  sprintf(portname,"dataport%i",channelNo);
  printf("portname= %s\n",portname);
  num_items = rconfig(portname,configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - dataport setting not found in config.ini\n");
    }
  else
    {
    printf("dataport CONFIG RESULT = '%s'\n",configresult);
    cc.dataPort = atoi(configresult);
    LH_DATA_IN_port[channelNo] = atoi(configresult);
    } 
     
  int s, i, slen=sizeof(si_DE), s1;

  if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		die("socket s");
	}
  int yes = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
         die("error setting sockopt reuseaddr for socket s\n");
    }

  if ( (s1=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		die("socket s1");
	}
  yes = 1; printf("setsockopt for s1\n");
  if (setsockopt(s1, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
         die("error setting sockopt reuseaddr for socket s1\n");
    }

  memset((char *) &si_DE, 0, sizeof(si_DE));
  memset((char *) &si_LH, 0, sizeof(si_LH));
  si_DE.sin_family = AF_INET;
  si_DE.sin_port = htons(DE_port);

  if (inet_aton(DE_IP , &si_DE.sin_addr) == 0) 
	{
		fprintf(stderr, "inet_aton() failed\n");
	}

		//send the message
  printf("M: Send CC...\n");
  if (sendto(s, &cc, sizeof(cc) , 0 , (struct sockaddr *) &si_DE, slen)==-1)
		{
			die("sendto()");
		}

  si_LH.sin_family = AF_INET;
  si_LH.sin_port = htons(cc.configPort);
  si_LH.sin_addr.s_addr = htonl(INADDR_ANY);

  printf("bind to s1\n");
  if( bind(s1 , (struct sockaddr*)&si_LH, sizeof(si_LH) ) == -1)
	{
		die("bind");
	}	
		//try to receive some data, this is a blocking call

  printf("M: Listening for AK to CC on port %i\n",cc.configPort);
  memset(&cc,'\0', sizeof(cc));
  if (recvfrom(s1, &cc, sizeof(cc), 0, (struct sockaddr *) &si_LH, &slen) == -1)
		{
			die("recvfrom()");
		}
		
  printf("M: DE responded to CC request; received from DE: %s, %i, %i, %i\n", cc.cmd, 
      cc.channelNo, cc.configPort, cc.dataPort);
  DE_CONF_IN_port[cc.channelNo] = cc.configPort;
  DE_DATA_IN_port[cc.channelNo] = cc.dataPort;
	
  close(s1);
  close(s);
  return 0;

}


/////////////////// UNIT TEST SETUP //////////////////////////////////

int max (int n1, int n2 )
{
   puts("test max");
   if ( n2 > n1 )  return n2;
   return n1;
}
int init_suite(void) { return 0; }
int clean_suite(void) { return 0; }
/************* Test case functions ****************/

void test_case_sample(void)
{
  // These first few tests simply verify that the test system is working.
   CU_ASSERT(CU_TRUE);
   CU_ASSERT_NOT_EQUAL(2, -1);
   CU_ASSERT_STRING_EQUAL("string #1", "string #1");
   CU_ASSERT_STRING_NOT_EQUAL("string #1", "string #2");

   CU_ASSERT(CU_FALSE);
   CU_ASSERT_EQUAL(2, 3);
   CU_ASSERT_STRING_NOT_EQUAL("string #1", "string #1");
   CU_ASSERT_STRING_EQUAL("string #1", "string #2");
}

void max_test_1(void) {
  CU_ASSERT_EQUAL( max(1,2), 2);
  CU_ASSERT_EQUAL( max(2,1), 2);
}

void max_test_2(void) {
  CU_ASSERT_EQUAL( max(2,2), 2);
  CU_ASSERT_EQUAL( max(0,0), 0);
  CU_ASSERT_EQUAL( max(-1,-1), -1);
}

void max_test_3(void) {
  CU_ASSERT_EQUAL( max(-1,-2), -1);
}

void test_get_config(void) {
  config_t cfg;
  config_setting_t *setting;

  char tresult[100];
  int tval = 0;
  int tme = 1;
 // puts("call rconfig");
  tval = rconfig("test_value", tresult, tme);
//  printf("returned value = '%s'",tresult);
 // printf("or %02x %02x %02x %02x %02x \n",tresult[0],tresult[1],tresult[2],tresult[3],tresult[4]);
  CU_ASSERT_EQUAL(tval,1);
  CU_ASSERT_STRING_EQUAL(tresult,"T123"); 

}
void test_data_prep(void) {
  char startDT[20];
  char endDT[20];
  char rbufp[50];
  char rdp[50];
  strcpy(startDT, "2020-06-08T00:00:00");
  strcpy(endDT, "2020-06-08T23:59:59");
  strcpy(rbufp,"/home/odroid/share1/TangerineData/uploads");
  int r = openConfigFile();
  num_items = rconfig("ramdisk_path",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - RAMdisk path setting not found in config.ini\n");
    }
  else
    {
    printf("RAMdisk path CONFIG RESULT = '%s'\n",configresult);
    strcpy(pathToRAMdisk,configresult);
    } 

//  int r = openConfigFile();
  int r1 = prep_data_files(startDT, endDT, rbufp);
  CU_ASSERT_EQUAL(r1,0);   // dummy statement for now
}

void date_test(void) {
  char theInput[300] = "HTTP/1.0 200 OK\nConnection: close\nContent-Length: 42\nContent-Type: text/html; charset=utf-8\nDate: Mon, 08 Jun 2020 20:40:53 GMT\nServer: waitress\n\nDR2020-06-08T00:00:00Z2020-06-08T23:59:59Z";
  char theStart[30];
  char theEnd[30];
  int r2 = getDataDates(theInput, &theStart[0], &theEnd[0]);
  CU_ASSERT_EQUAL(r2,1);  // here we have found a DR command
  printf("In test, endpoint = %s\n", theEnd);
  printf("In test, startpoint = %s\n", theStart);
  CU_ASSERT_STRING_EQUAL(theStart, "2020-06-08T00:00:00");
  CU_ASSERT_STRING_EQUAL(theEnd, "2020-06-08T23:59:59");
  strcpy(theInput, "HTTP/1.0 200 OK\nConnection: close\nContent-Length: 42\nContent-Type: text/html; charset=utf-8\nDate: Mon, 08 Jun 2020 20:40:53 GMT\nServer: waitress\n\n");
  r2 = getDataDates(theInput, &theStart[0], &theEnd[0]);
  CU_ASSERT_EQUAL(r2,0);  // here we have found no command
}

void buildFileName_test(void) {
// test assumes the tested code will complete in same second as test code runs
// set up config file for the test

  int r = openConfigFile();
  char theNode[10] = "";
  char theGrid[10] = "";
  printf("look in config for node\n");
  int num_items = rconfig("node",theNode,0);
  printf("node found: %s\n",theNode);
  num_items = rconfig("grid",theGrid,0);

//
 char fn[75] = "";
 strcpy(fn, (char *)buildFileName("N1234", "EM63fj"));
 printf("computed filename = '%s'\n", fn);
 char testfilename[100] = "";
 time_t t= time(NULL);
 struct tm *tm = localtime(&t);
 char s[64];
 assert(strftime(s, sizeof(s), "%FT%H%M%SZ", tm));
 strcpy(testfilename, s);
 strcat(testfilename, "_N1234_T1_EM63fj_DRF.tar");
 strcat(testfilename, "\0");
 CU_ASSERT_STRING_EQUAL(fn, testfilename);
}

void testUploadThread(){
  int r = openConfigFile();
  int uplrc = 0;
  long h= 60;
  void *ret;
  pthread_t uplthread;
  printf("M: Start upload thread\n");
  uplrc = pthread_create(&uplthread, NULL, dataUpload, (void*)h);
  printf("thread rd %i\n",uplrc);
  uplrc = pthread_join(uplthread, &ret);
  printf("M: join complete\n");
}

void testCreateChannel() {
  printf("NOTE: If any of the following tests hang, it means DE is not responding.\n");
  printf("1. Test CC - will only pass if DE is connected & working\n");
  printf("2. TEST to see if there is a DE on the network\n");
  DE_port = 0;
  discover_DE();
  CU_ASSERT_NOT_EQUAL(DE_port,0);  // did we find a DE?
  printf("3. TEST found DE at IP %s Port B = %d\n",DE_IP,DE_port);
  // section to test channel 0
  int channelNo = 0;
  DE_CONF_IN_port[0] = 0;
  printf("4. TEST create_channel; see if DE responds to Create Channel %i request\n", channelNo);
  create_channel(channelNo);
  CU_ASSERT_NOT_EQUAL(DE_CONF_IN_port[0],0);
  sleep(0.2);

// Section to test channel 1
  channelNo = 1;
  DE_CONF_IN_port[1] = 0;
  printf("*** 5. TEST to see if DE responds to Create Channel %i request\n", channelNo);
  create_channel(channelNo);
  CU_ASSERT_NOT_EQUAL(DE_CONF_IN_port[1],0);

}

void testConfigureChannels() {
  printf("\n*** 6. Test CH 0 - will only pass if Test CC worked\n");
  sleep(0.2);
  int channelNo = 0;
  
  int rc = makeCHrequest(channelNo);
  CU_ASSERT_EQUAL(rc,0);

  printf("\n*** 7. Test CH 1 - will only pass if Test CC worked\n");
  sleep(0.2);
  channelNo = 1;
  rc= makeCHrequest(channelNo);
  CU_ASSERT_EQUAL(rc,0);

}

void testDataRateReq() {
  printf("\n8. TEST Rate Request\n");
  dataRatesReceived = 0;
  sendCommandToDE(0, "R?");
  sleep(0.5);
  CU_ASSERT_NOT_EQUAL(dataRatesReceived, 0);

}

void testprocessUserActions(){
  printf("\n9. TEST processUserActions coming from flask\n");
  printf("Start thread for processing of user actions\n");
  // This thread normally watches for commands to come thru socket from
  // flask, to execute user requests. Here we mock the flask app.py 
  pthread_t threadID;
  int err = pthread_create(&threadID, NULL, &processUserActions, NULL);
  CU_ASSERT_EQUAL(err,0);
  printf("thread creation retcode=%i; waiting 2 sec\n",err);
  sleep(2.2);  // wait for thread to get started

    int sockfd, connfd; 
    struct sockaddr_in servaddr, cli; 
  
    // socket create and varification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("Test package socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("Test Package Socket successfully created..\n"); 
    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(6100); 
    sleep(0.2); // wait for accept to be in place at 
    // connect the client socket to server socket 
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
        printf("Test package connection with the server failed...\n"); 
        exit(0); 
    } 
    else
        {
        char buff[80];
        struct commandBuf cmdBuf;
        printf("Test package connected to the port..\n"); 
        sleep(0.2);
/*
        memset(&cmdBuf,0,sizeof(cmdBuf));
        memcpy(&cmdBuf.cmd,"S?",2); 
        printf("STATUS_INQUIRY using command '%s'\n",cmdBuf.cmd);
        DE_OK = 0;
        int w=write(sockfd, &cmdBuf, sizeof(cmdBuf)); 
        printf("Test package wrote %i bytes\n",w);
        sleep(2.5);
        printf("After STATUS INQ, DE_OK = %i\n",DE_OK);
        CU_ASSERT_EQUAL(DE_OK,1);  // did DE answer OK?
*/
        memset(&cmdBuf,0,sizeof(cmdBuf));
        memcpy(&cmdBuf.cmd,START_DATA_COLL,2); 
        cmdBuf.channelNo = 0;
        printf("START_DATA_COLL using command '%s'\n",cmdBuf.cmd);
      //  bzero(buff, sizeof(buff));
      //  strcpy(buff,"SC");

        int w1=write(sockfd, &cmdBuf, sizeof(cmdBuf));
        printf("Test Package Wrote %i bytes to socket sockfd\n",w1);

        sleep(10.0);   // wait for reply
/*
        DE_OK = 0;
        memset(&cmdBuf,0,sizeof(cmdBuf));
        memcpy(&cmdBuf.cmd,"S?",2); 
        printf("STATUS_INQUIRY using command '%s'\n",cmdBuf.cmd);
        DE_OK = 0;
        w=write(sockfd, &cmdBuf, sizeof(cmdBuf)); 
        printf("Test package wrote %i bytes\n",w);
        sleep(2.5);
        printf("After STATUS INQ, DE_OK = %i\n",DE_OK);
*/


        memcpy(&cmdBuf.cmd,STOP_DATA_COLL,2);
        cmdBuf.channelNo = 0;
        printf("STOP_DATA_COLL using command '%s', channel %i\n",cmdBuf.cmd,cmdBuf.channelNo);
        int w2=write(sockfd, &cmdBuf, sizeof(cmdBuf));
        printf("Test Package Wrote %i bytes to socket sockfd\n",w2);
        sleep(2);  // must wait at end so mainctl doesn't halt before DE finishes

        memset(&cmdBuf,0,sizeof(cmdBuf));
        memcpy(&cmdBuf.cmd,START_FT8_COLL,2); 
        cmdBuf.channelNo = 1;
        printf("START_FT8_COLL using command '%s', %x %x  chnl#=%i\n",cmdBuf.cmd, 
              cmdBuf.cmd[0], cmdBuf.cmd[1], cmdBuf.channelNo);
      //  bzero(buff, sizeof(buff));
      //  strcpy(buff,"SC");

        int w3=write(sockfd, &cmdBuf, sizeof(cmdBuf));
        printf("Test Package Wrote %i bytes to socket sockfd\n",w3);
        sleep(10);
        memcpy(&cmdBuf.cmd,STOP_FT8_COLL,2);
        cmdBuf.channelNo = 1;
        printf("STOP_DATA_COLL (FT8)using command '%s', channel %i\n",cmdBuf.cmd,cmdBuf.channelNo);
        int w4=write(sockfd, &cmdBuf, sizeof(cmdBuf));
        printf("Test Package Wrote %i bytes to socket sockfd\n",w4);
        sleep(2);  // must wait at end so mainctl doesn't halt before DE finishes

        }
  
    close(sockfd);

}




//////////////////////////////////////////////////////////////////////////
///////////  Built-in Unit Tests
/////////////////////////////////////////////////
int run_all_tests()
{
  {
	  {
	  printf("UNIT TESTING mainctl\n");

	  CU_pSuite pSuite = NULL;

   // initialize the CUnit test registry 
   	  if ( CUE_SUCCESS != CU_initialize_registry() )
        return CU_get_error();

   // add a suite to the registry 
   	  pSuite = CU_add_suite( "max_test_suite", init_suite, clean_suite );
   	  if ( NULL == pSuite ) {
        CU_cleanup_registry();
        return CU_get_error();
   }

   // add the tests to the suite 
   	  if ((NULL == CU_add_test(pSuite, "max_test_1", max_test_1)) ||
          (NULL == CU_add_test(pSuite, "max_test_2", max_test_2))  ||
          (NULL == CU_add_test(pSuite, "max_test_3", max_test_3))  ||
		  (NULL == CU_add_test(pSuite, "config_test1", test_get_config)) ||
          (NULL == CU_add_test(pSuite, "data_prep_test", test_data_prep)) ||
          (NULL == CU_add_test(pSuite, "date_extract", date_test)) ||
          (NULL == CU_add_test(pSuite, "buildFileName", buildFileName_test)) ||
          (NULL == CU_add_test(pSuite, "testUploadThread", testUploadThread)) ||
          (NULL == CU_add_test(pSuite, "testCreateChannal", testCreateChannel)) ||
          (NULL == CU_add_test(pSuite, "testConfigureChannels", testConfigureChannels)) ||
          (NULL == CU_add_test(pSuite, "testDataRateReq", testDataRateReq)) ||
          (NULL == CU_add_test(pSuite, "testprocessUserActions", testprocessUserActions))


      )
   {
      CU_cleanup_registry();
      return CU_get_error();
   }

   // Run all tests using the basic interface
   CU_basic_set_mode(CU_BRM_VERBOSE);
   CU_basic_run_tests();
   printf("\n");
   CU_basic_show_failures(CU_get_failure_list());
   printf("\n\n");

/*   TODO: set up the complete test suite below
   // Run all tests using the automated interface
   CU_automated_run_tests();
   CU_list_tests_to_file();

   // Run all tests using the console interface
   CU_console_run_tests();
*/

   // Clean up registry and return
   CU_cleanup_registry();
   return CU_get_error();

	  }
	}
}

///////////////// end of test package ///////////////////////


/*  ******************** MAIN PROGRAM **************************** */
int main(int argc, char *argv[]) {
// Discover what devices are acessible and respond to discovery packet (preamble hex EF FE ) 
  puts("M:  $$$$$$$$$$$$$ mainctl starting $$$$$$$$$$$$");
  int testresult;


  if(argc > 1)  // execute only this if user asked to run the unit tests
    {
    printf("argc = %d\n",argc);
    printf("Got argument: '%s'\n",argv[1]);
    if(strcmp(argv[1],"test")==0)
      testresult = run_all_tests();
      return testresult;
    }

  puts("UDPdiscovery    ******    *****");

  //DEdevice = UDPdiscoverNew();  // call UDP discovery rtn

// here we look for compatible device
  LH_port = 0;
  discovered[0]= UDPdiscover(&LH_port);    // pass handle to outbound port
  // discovery randomly selects an outbound port. DE will talk to that port.
  // Here we find out what that port is, so we can listen on it.
  // In documentation, this port is called "Port A"
  printf("discovery selected LH port %d (Port A)\n", LH_port); 
  puts("Here's what we found:");
  for(int i=0;i<MAX_DEVICES;i++)
	{
	 if(strlen(discovered[i].name) == 0 ) break;   // apparently the end of the list
     fprintf(stderr,"  ^^^^^^^ Device %d is %s at %s port %u  \n",i, discovered[i].name,
	   inet_ntoa(discovered[i].info.network.address.sin_addr),   
       htons(discovered[i].info.network.address.sin_port));
    // TODO: temporary way to pick device on the wire; 
    // will need to have a better way to select from multiple responding devices
	if(discovered[i].device == DEVICE_TANGERINE) 
	 {
		strcpy(DE_IP, inet_ntoa(discovered[i].info.network.address.sin_addr));
		DE_port = htons(discovered[i].info.network.address.sin_port);  // this is Port B
		printf("Selected Tangerine at IP %s port %u (Port B)\n",DE_IP, DE_port);
	  }
	}


  char DE_ip_str[20];
  packetCount = 0;
  int DE_port;
  int controller_port;
  int rc = openConfigFile();


  // This is port where we receive TCP control data from app.py
  num_items = rconfig("controlport",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - controlport setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT for control port= '%s'\n",configresult);
    printf("len =%lu\n",strlen(configresult));
    controller_port = atoi(configresult);
    }

  num_items = rconfig("ringbuffer_path",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - ringbuffer_path setting not found in config.ini");
    }
  else
    { strcpy(ringbuffer_path, configresult);
    }
    printf(" CONFIG RESULT for ringbuffer path= '%s'\n",configresult);
    printf("len =%lu\n",strlen(configresult));
   

  num_items = rconfig("central_host",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - central host setting not found in config.ini\n");
    }
  else
    {
    printf("central host CONFIG RESULT = '%s'\n",configresult);
    strcpy(central_host, configresult);
    }

  num_items = rconfig("central_port",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - central port setting not found in config.ini\n");
    }
  else
    {
    printf("central port CONFIG RESULT = '%s'\n",configresult);
    central_port = atoi(configresult);
    }  

  num_items = rconfig("ramdisk_path",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - RAMdisk path setting not found in config.ini\n");
    }
  else
    {
    printf("RAMdisk path CONFIG RESULT = '%s'\n",configresult);
    strcpy(pathToRAMdisk,configresult);
    } 

  num_items = rconfig("firehoser_path",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - FirehoseR path setting not found in config.ini\n");
    }
  else
    {
    printf("FirehoseR path CONFIG RESULT = '%s'\n",configresult);
    strcpy(firehoseR_path,configresult);
    } 

  // Send CREATE CHANNEL to DE for up to 3 channels  TODO: add WSPR
  for(int channel=0;channel < 2;channel++)
    {
    DE_CONF_IN_port[channel] = 0;
    printf("Create Channel %i request\n", channel);
    create_channel(channel);
    sleep(0.2);
    }
  // send CONFIGURE CHANNELS to set up the created channels
  int crc;
  for(int channel=0;channel < 2;channel++)
    {
    crc = makeCHrequest(channel);
    printf("Config channel rc for channel %i = %i\n",channel, rc);
    sleep(0.2);
    }

  // Start thread for processing user actions, i.e., commands arriving
  // on TCP port from app.py

   printf("M: Start thread for processing of user actions\n");

   pthread_t threadID;
   int err = pthread_create(&threadID, NULL, &processUserActions, NULL);

   sleep(0.5);
  dataRatesReceived = 0;
  sendCommandToDE(0, "R?");
  sleep(0.5);

   pthread_join(threadID, NULL);  // only returns here if action is to quit




///////////////////////////////////////////////////////////////////////

  printf("Try to open directory '%s'\n",ringbuffer_path);
  DIR* dir = opendir(ringbuffer_path);
  if(dir)
	{
	printf("Digital RF directory found\n");
	}
  else if (ENOENT == errno)
	{ printf("Digital RF directory not found; try to create one\n");
	//FILE *fp;
	//char *command;
	//strcpy(command, "mkdir /tmp/hdf5/junk0 \n");
// will need config items to point to both upper & lower level directories

// TODO:  this has to check & create directory according to the configured path, not the fixed one

	FILE* fp1 = popen("mkdir /tmp/hdf5", "r");
	FILE* fp2 = popen("mkdir /tmp/hdf5/junk0", "r");
	pclose(fp1);
	pclose(fp2);
	}

    int hbrc = 0;
    long h;
  int heartbeat_interval = 60;  // seconds
  num_items = rconfig("heartbeat_interval",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - heartbeat_interval setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT = '%s'\n",configresult);
    printf("len =%lu\n",strlen(configresult));
    heartbeat_interval = atoi(configresult);
    }


////////////////////////////////////////////////////////////////
	

  return(0);
}
