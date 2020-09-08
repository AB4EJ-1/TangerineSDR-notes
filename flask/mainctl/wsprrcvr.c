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

int wspractive[8];    // flag to indicate if a given wspr channel is collecting data
int wsprcounter[8];   // counter of how many wspr samples saved in this collection period
int inputcount[8];    
int upload = 0;      // set to 1 if user wants to upload spots to PSKReporter

float chfrequency[8];
int wspractive[8];      // indicates if wspr has started for this streamNo

//static char pathToRAMdisk[100] = "/mnt/RAM_disk";  // temp hard-coded

extern char rconfig(char * arg, char * result, int testThis);

int main() { 

  char pathToRAMdisk[100];
  char configresult[100];
  char mycallsign[20];
  char mygrid[20];
  char myantenna0[50];  // there is length limit on these in upload-to-pskreporter
  char myantenna1[50];
  // for c2 header
  char zeros[15] = "000000_000.c2";
  int32_t type = 2;

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

  num_items = rconfig("antenna0",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - antenna0 setting not found in config.ini\n");
    }
  else
    {
    printf("antenna0 CONFIG RESULT = '%s'\n",configresult);
    strcpy(myantenna0,configresult);
    } 
  printf("antenna0 =%s\n",myantenna0);

  num_items = rconfig("psk_upload",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - psk_upload setting not found in config.ini\n");
    }
  else
    {
    printf("psk_upload CONFIG RESULT = '%s'\n",configresult);
    if(strncmp(configresult, "On", 2) == 0)
      upload = 1;
    else
      upload = 0;
    } 
  printf("antenna0 =%s\n",myantenna0);

    num_items = rconfig("dataport2",configresult,0);
    LH_DATA_IN_port = atoi(configresult);


    printf("Starting WSPR receiving, port=%i\n",LH_DATA_IN_port);
    int streamID = 0;
	int sockfd; 
	struct sockaddr_in servaddr, cliaddr; 
    int idialfreq = 0;

    for(int i=0; i<8; i++)  // Go thru 8 possible channels & set up
      {
      wspractive[i] = 0;  // mark them all inactive to start
      inputcount[i] = 0;
      char channel_no[8];
      char result[5]="";
      sprintf(channel_no,"wsant%i",i);


  num_items = rconfig(channel_no,configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - channel setting not found in config.ini\n");
    }
  else
    {
    printf("%s CONFIG RESULT = '%s'\n",channel_no,configresult);
    strcpy(result,configresult);
     
    if(strncmp(result,"0",1) == 0 | strncmp(result,"1",1) ==0)
      {
      sprintf(channel_no,"ws%if",i);
      num_items = rconfig(channel_no,configresult,0);
      if(num_items == 1)
        dialfreq[i] = atof(configresult);
        idialfreq = atoi(configresult);
        printf("dialfreq %i %f \n",i,dialfreq[i]);
      }
     }
    }

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

    while(1) // loop until process is killed
     {
      int recording_active = 0;
	  n = recvfrom(sockfd, &wsprbuffer, BUFSIZE, 
				MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
				&len);
     streamID = (int)wsprbuffer.stream_ID[3];  // TODO: check this, it is hard coded
     
     time(&rawtime);
     info = gmtime(&rawtime);
     int seconds = info->tm_sec;
     int minute = info->tm_min;
     printf("WSPR countdown = %i:%i \n",minute,seconds);
     if(minute % 2 == 0 && (seconds == 1 | seconds == 2 | seconds == 3) )  // is it the top of an even minute?
      {
      recording_active = 1; // start recording
      }
     else
      {
      continue;  // skip processing
      }
    
    int first_buffer = 1;
    while(recording_active)
     {
     if(!first_buffer)
       {
	   n = recvfrom(sockfd, &wsprbuffer, BUFSIZE, 
				MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
				&len);
      streamID = (int)wsprbuffer.stream_ID[3];  // TODO: check this, it is hard coded
       }
      else
       {
       printf("In recording_active section\n");
       }
    
      first_buffer = 0;

     time(&rawtime);
     info = gmtime(&rawtime);
     int seconds = info->tm_sec;
     int minute = info->tm_min;


     printf("WSRC: time: %i:%i sample count %i\n",minute,seconds,wsprcounter[streamID]);
     
     
	// printf("streamID : %i\n", (int)wsprbuffer.stream_ID[3]); 
     // do this for the first sample
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
        sprintf(name[streamID], "%s/WSPR/wspr_%i_%i_%d_%s.c2", pathToRAMdisk, streamID, idialfreq,1,date); 

       printf("create raw data WSPR file %s\n",name[streamID]);
       if((fp[streamID] = fopen(name[streamID], "wb")) == NULL)
         { fprintf(stderr,"Could not open file %s \n",name[streamID]);
          return -1;
         }

       // c2 header
       fwrite(zeros, 1,14, fp[streamID]);
       fwrite(&type, 1, 4, fp[streamID]);
       double dialfreq1 = dialfreq[0];  // TODO: temporary
       printf("WSRC: freq = %f\n",dialfreq[0]);  // TODO: temp value
       fwrite(&dialfreq1, 1, sizeof(dialfreq1), fp[streamID]);


       }
  // when we drop thru to here, we are ready to start recording data

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
         // trigger processing of the wspr data file

           printf("WSPR decoding...\n");
           char chstr[4];
           sprintf(chstr,"%i",streamID);
           char mycmd[200];
 
           int ret = system(mycmd);

           sprintf(mycmd,"./wsprd -JC 5000 -f %f %s > %s/WSPR/decoded%i.txt",dialfreq[streamID],name[streamID],pathToRAMdisk,streamID);
           printf("issue command: %s\n",mycmd);
           // Note: this assumes that decoder (wsprd_del) deletes work file when done.
           ret = system(mycmd);
           printf("wspr decode ran, rc = %i\n",ret);
         // for complete list of all WSPR decodes, see /home/odroid/projects/TangerineSDR-notes/flask/ALL_WSPR.TXT
         //  sprintf(mycmd,"cat /mnt/RAM_disk/WSPR/decoded3.txt  >> /mnt/RAM_disk/WSPR/decode_hist.txt");
         //  ret = system(mycmd);

           sprintf(mycmd,"sort -nr -k 4,4 %s/WSPR/decoded%i.txt | awk '!seen[$1\"_\"$2\"_\"int($6)\"_\"$7] {print} {++seen[$1\"_\"$2\"_\"int($6)\"_\"$7]}' | sort -n -k 1,1 -k 2,2 -k 6,6 -o  %s/WSPR/decoded%iz.txt",pathToRAMdisk,streamID,pathToRAMdisk,streamID);
           ret = system(mycmd);
           printf("issue command: %s\n",mycmd);
           sprintf(mycmd,"curl -sS -m 30 -F allmept=@\"%s/WSPR/decoded%iz.txt\" -F call=AB4EJ -F grid=EM63fj https://wsprnet.org/meptspots.php", pathToRAMdisk,streamID);
           ret = system(mycmd);
           printf("issue command: %s\n",mycmd);

           recording_active = 0;

/*
           if(upload == 1)
             {
             printf("Upload to PSKReporter\n");
             sprintf(mycmd,"./mainctl/upload-to-pskreporter %s %s %s %s/wspr/decoded%d.txt", 
                mycallsign, mygrid, myantenna0, pathToRAMdisk, streamID);
             ret = system(mycmd);
             printf("psk upload ran, rc = %i\n",ret);
             }
*/
/*
    do  {  // wait unti top of next even minute
       time_t rawtime;
       struct tm * info ;
       time(&rawtime);
       info = gmtime(&rawtime);
       int seconds = info->tm_sec;
       int minute = info->tm_min;
       printf("WRCVR: countdown to start WSPR %i:%i\n",minute,seconds);
       if (seconds == 0 && minute % 2 == 0)
         {
         break;
         }
       sleep(1);

        } while(1);
*/


          
           }
         }
     
       } // end of recording_active loop
      } // end of while(1) loop

	return 0; 
} 

