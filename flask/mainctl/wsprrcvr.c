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

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <time.h>
#include <math.h>
#include <complex.h>
#include "de_signals.h"
#include <pthread.h>

// #define PORT	 40003   // TODO: needs to be computed/configurable
#define BUFSIZE  8300
#define WSPRFSIZE 42000   // # samples collected before invoking decoder

static struct VITAdataBuf wsprbuffer;

static uint16_t LH_DATA_IN_port;  // port F; LH listens for spectrum data on this port

// variables for wspr reception
char date[12];
char name[8][64];
time_t t;
struct tm *gmt;
FILE *fp[8];
float complex IQval;
double dialfreq[8];  // array of dial frequencies for wspr
int idialfreq[8];

int wspractive[8];    // flag to indicate if a given wspr channel is collecting data
int wsprcounter[8];   // counter of how many wspr samples saved in this collection period
int inputcount[8];    
int upload = 0;      // set to 1 if user wants to upload spots to PSKReporter

float chfrequency[8];


static char pathToRAMdisk[100];

extern char rconfig(char * arg, char * result, int testThis);


///////////////////////////////////////////////////////////////////////////////
void *  processWorkfile(void * streamID){
 // int streamIDt = *((int *) streamID);
  int streamIDt = (uint64_t)streamID;

  printf("Start processWorkfile thread, stream %i\n", streamIDt);
  

       //   printf("WSPR decoding...\n");
           char chstr[4];
           sprintf(chstr,"%i",streamIDt);
           char mycmd[200];
 
           int ret = system(mycmd);

           sprintf(mycmd,"nice -n10 ./wsprd -JC 5000 -f %f %s > %s/WSPR/decoded%i.txt",dialfreq[streamIDt],name[streamIDt],pathToRAMdisk,streamIDt);
           printf("issue command: %s\n",mycmd);
           // Note: this assumes that decoder (wsprd_del) deletes work file when done.
           ret = system(mycmd);
           printf("wspr decode ran, rc = %i\n",ret);

 // TODO: following section to run only if wspr_upload = On in config.ini

         // for complete list of all WSPR decodes, see /home/odroid/projects/TangerineSDR-notes/flask/ALL_WSPR.TXT

           sprintf(mycmd,"nice -n10 sort -nr -k 4,4 %s/WSPR/decoded%i.txt | awk '!seen[$1\"_\"$2\"_\"int($6)\"_\"$7] {print} {++seen[$1\"_\"$2\"_\"int($6)\"_\"$7]}' | sort -n -k 1,1 -k 2,2 -k 6,6 -o  %s/WSPR/decoded%iz.txt",pathToRAMdisk,streamIDt,pathToRAMdisk,streamIDt);
           printf("issue command: %s\n",mycmd);
           ret = system(mycmd);

           sprintf(mycmd,"nice -n9 curl -sS -m 30 -F allmept=@\"%s/WSPR/decoded%iz.txt\" -F call=AB4EJ -F grid=EM63fj https://wsprnet.org/meptspots.php", pathToRAMdisk,streamIDt);
           printf("issue command: %s\n",mycmd);
           ret = system(mycmd);
           sprintf(mycmd,"rm %s",name[streamIDt]);  // delete the work file
           printf("issue command: %s\n",mycmd);
           ret = system(mycmd);

           printf("End of processingWorkfile, stream %i\n",streamIDt);

} // end of processWorkfile thread


int main() { 

 // char pathToRAMdisk[100];
  char configresult[100];
  char mycallsign[20];
  char mygrid[20];
//  char myantenna0[50];  // there is length limit on these in upload-to-pskreporter
 // char myantenna1[50];
  // for c2 header
  char zeros[15] = "000000_000.c2";
  int32_t type = 2;
  int recording_active = 0;
  int num_items = 0;
  printf("wsprrcvr start\n");
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
  printf("Ramdisk path =%s\n",pathToRAMdisk);

  num_items = rconfig("callsign",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - callsign setting not found in config.ini\n");
    }
  else
    {
    printf("callsign CONFIG RESULT = '%s'\n",configresult);
    strcpy(mycallsign,configresult);
    } 
  printf("callsign =%s\n",mycallsign);

  num_items = rconfig("grid",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - grid setting not found in config.ini\n");
    }
  else
    {
    printf("grid CONFIG RESULT = '%s'\n",configresult);
    strcpy(mygrid,configresult);
    } 
  printf("grid =%s\n",mygrid);


  num_items = rconfig("wspr_upload",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - wspr_upload setting not found in config.ini\n");
    }
  else
    {
    printf("wspr_upload CONFIG RESULT = '%s'\n",configresult);
    if(strncmp(configresult, "On", 2) == 0)
      upload = 1;
    else
      upload = 0;
    } 


    num_items = rconfig("dataport2",configresult,0);
    LH_DATA_IN_port = atoi(configresult);


    printf("Starting WSPR receiving, port=%i\n",LH_DATA_IN_port);
    int streamID = 0;
	int sockfd; 
	struct sockaddr_in servaddr, cliaddr; 

    for(int i=0; i<8; i++)  // Go thru 8 possible channels & set up
      {
      wspractive[i] = 0;  // mark them all inactive to start
      inputcount[i] = 0;
      char subchannel_no[8];
      char result[5]="";
      // here we check to see which WSPR antenna ports are set to 0 to 1 (i.e., not Off)
      sprintf(subchannel_no,"wsant%i",i);
      num_items = rconfig(subchannel_no,configresult,0);
      if(num_items == 0)
        {
        printf("ERROR - channel setting not found in config.ini\n");
        }
      else
        {
        printf("%s CONFIG RESULT = '%s'\n",subchannel_no,configresult);
        strcpy(result,configresult);
     // Get the dial freq. for activated subchannels
        if(strncmp(result,"0",1) == 0 | strncmp(result,"1",1) ==0)
          {
          sprintf(subchannel_no,"ws%if",i);
          num_items = rconfig(subchannel_no,configresult,0);
          if(num_items == 1)
            dialfreq[i] = atof(configresult);
            idialfreq[i] = atoi(configresult);  // integer value of this for use in file name
            printf("WSPR dialfreq %i %f \n",i,dialfreq[i]);
          }
         }
      }  // end of loop for going thru channels

	// Create socket file descriptor 
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
		perror("socket creation failed"); 
		exit(EXIT_FAILURE); 
	  } 
	
	memset(&servaddr, 0, sizeof(servaddr)); 
	memset(&cliaddr, 0, sizeof(cliaddr)); 
	
	// Filling server information 
	servaddr.sin_family = AF_INET; // IPv4 
	servaddr.sin_addr.s_addr = INADDR_ANY; 
	servaddr.sin_port = htons(LH_DATA_IN_port); 
	
	// Bind the socket with the server address 
	if ( bind(sockfd, (const struct sockaddr *)&servaddr, 
			sizeof(servaddr)) < 0 ) 
	{ 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	int len, n; 

	len = sizeof(cliaddr); //len is value/resuslt 

   time_t rawtime;
   struct tm * info ;

// The following while loop processes all packets coming in to this LH_DATA_in port.
// There may be multiple streams (each one has its own stream ID); these are handled
// as subchannels of WSPR data. The 4th byte (wsprbuffer.stream_ID[3]) is stream ID,
// which is standard location for this in VITA format.

    while(1) // loop until process is killed
     {

	  n = recvfrom(sockfd, &wsprbuffer, BUFSIZE, 
				MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
				&len);
     streamID = (int)wsprbuffer.stream_ID[3];  
     recording_active = 0;
     time(&rawtime);
     info = gmtime(&rawtime);
     int seconds = info->tm_sec;
     int minute = info->tm_min;
     int countdownMin = 0;
     if(minute % 2 == 0)
       { countdownMin = 1; }

     int countdownSec = 60 - seconds;
     printf("WSPR decoding for stream %i will start in %i min. %i sec.\n",streamID,countdownMin,countdownSec);
    // we start recording if this buffer was created at top of an even minute
     if(minute % 2 == 0 && (seconds == 1 | seconds == 2 | seconds == 3) ) 
      {
      recording_active = 1; // start recording
      }
     else
      {
      continue;  // skip processing
      }
    
    int first_buffer = 1;
    while(recording_active)  // this loop processes all packets for the 2 min. recording interval
     {
     if(!first_buffer)  // do we already have the first packet?
       {  // this gets all remaining packets after the first
	   n = recvfrom(sockfd, &wsprbuffer, BUFSIZE, 
				MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
				&len);
      streamID = (int)wsprbuffer.stream_ID[3];  // we will interleave packets when > 1 channel running.
       }
      else
       {
       printf("In recording_active section\n");
       first_buffer = 0;  // set so that subsequent passes thru loop do the recvfrom
       }
    
     time(&rawtime);
     info = gmtime(&rawtime);
     int seconds = info->tm_sec;
     int minute = info->tm_min;

     printf("WSRC: time: %i:%i streamID %i,sample count %i\n",minute,seconds,streamID,wsprcounter[streamID]);
    
     // do this for the first packet of each subchannel
     if(wspractive[streamID] == 0) //this wspr stream not yet active
      {
       wspractive[streamID] = 1;   // mark this wspr stream as active
       wsprcounter[streamID] = 0;            // zero this counter
       inputcount[streamID] = 0;
       printf("\nSaving WSPR for decode\n");

       t = time(NULL);
       if((gmt = gmtime(&t)) == NULL)
          { fprintf(stderr,"Could not convert time\n"); }
       strftime(date, 12, "%y%m%d_%H%M", gmt);
       sprintf(name[streamID], "%s/WSPR/wspr_%i_%i_%d_%s.c2", pathToRAMdisk, streamID, idialfreq[streamID],1,date); 

       printf("create raw data WSPR file %s\n",name[streamID]);
       if((fp[streamID] = fopen(name[streamID], "wb")) == NULL)
         { fprintf(stderr,"Could not open file %s \n",name[streamID]);
          return -1;
         }

       // c2 header
       fwrite(zeros, 1,14, fp[streamID]);
       fwrite(&type, 1, 4, fp[streamID]);
       double dialfreq1 = dialfreq[streamID];  // TODO: this may change with Tangerine DE; check
       printf("WSRC: stream ID %i freq = %f\n",streamID,dialfreq[streamID]); 
       fwrite(&dialfreq1, 1, sizeof(dialfreq1), fp[streamID]); // write dial freq into work file

       }

  // when we drop thru to here, we are ready to start recording data

       // process the 1024 complex samples in this packet
       for(int i=0; i < 1024 && wsprcounter[streamID] <= WSPRFSIZE; i++)   // go thru input buffer
         {
         inputcount[streamID]++;

         IQval = wsprbuffer.theDataSample[i].I_val + (wsprbuffer.theDataSample[i].Q_val * I);
         IQval = IQval / 1000000.0;  // experimental; TODO: remove this
         fwrite(&IQval , 1, sizeof(IQval), fp[streamID]);
         wsprcounter[streamID]++;

         // if we get another buffer or 2 beyond the last one, we ignore it
         if(wsprcounter[streamID] >= WSPRFSIZE)   // have we filled output?
           {

           if(wsprcounter[streamID] >= (WSPRFSIZE+3000))  // have we already done this?
             {
             break;
             }
           IQval = 0.0 + (0.0 * I);
           for(int k = 0; k < 3000; k++)  // pad end of file with zeros
             {
             fwrite(&IQval , 1, sizeof(IQval), fp[streamID]);
             wsprcounter[streamID]++;
             }


           wspractive[streamID] = 0;  // mark it inactive
           fclose(fp[streamID]);


        //   sprintf(mycmd,"nice -n10 ./wsprd -JC 5000 -f %f %s > %s/WSPR/decoded%i.txt",dialfreq[streamIDt],name[streamIDt],pathToRAMdisk,streamIDt);



           char mycmd[200];
         //  int idialfreq = dialfreq[streamID];
           sprintf(mycmd, "sh ./decode_and_send.sh %s %i %s %s %f %s  &",pathToRAMdisk,streamID,"AB4EJ","EM63fj",dialfreq[streamID],name[streamID]);
      //     TODO: the above must use configured values
           printf("WSRC: issue command %s\n",mycmd);
           int ret = system(mycmd);
           printf("WSRC: decoding for stream %i started\n",streamID);
           


           for(int i = 0; i < 8; i++)
             {
             recording_active = recording_active | wspractive[i];
             }

          
           }
         }
     
       } // end of recording_active loop
      } // end of while(1) loop

	return 0; 
} 

