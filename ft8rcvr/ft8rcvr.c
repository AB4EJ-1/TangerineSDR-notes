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

#define PORT	 40003   // TODO: needs to be computed/configurable
#define BUFSIZE  8300
#define FT8FSIZE 236000   // # samples collected before invoking decoder

static struct VITAdataBuf ft8buffer;

// variables for FT8 reception
char date[12];
char name[8][64];
time_t t;
struct tm *gmt;
FILE *fp[8];
float complex IQval;
double dialfreq[8];  // array of dial frequencies for FT8

int ft8active[8];    // flag to indicate if a given ft8 channel is collecting data
int ft8counter[8];   // counter of how many ft8 samples saved in this collection period
int inputcount[8];

float chfrequency[8];
int ft8active[8];      // indicates if ft8 has started for this streamNo

//static char pathToRAMdisk[100] = "/mnt/RAM_disk";  // temp hard-coded

extern char rconfig(char * arg, char * result, int testThis);

int main() { 

  char pathToRAMdisk[100];
  char configresult[100];
  char mycallsign[20];
  char mygrid[20];
  char myantenna0[50];  // there is length limit on these in upload-to-pskreporter
  char myantenna1[50];

  int num_items = 0;
  printf("ft8rcvr start\n");
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


    printf("Starting FT8 receiving, port=%i\n",PORT);
    int streamID = 0;
	int sockfd; 
	struct sockaddr_in servaddr, cliaddr; 
    int idialfreq = 0;

    for(int i=0; i<8; i++)  // Go thru 8 possible channels & set up
      {
      ft8active[i] = 0;  // mark them all inactive to start
      inputcount[i] = 0;
      char channel_no[8];
      char result[5]="";
      sprintf(channel_no,"ftant%i",i);


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
      sprintf(channel_no,"ft8%if",i);
      num_items = rconfig(channel_no,configresult,0);
      if(num_items == 1)
        dialfreq[i] = atof(configresult) * 1000000;
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
	servaddr.sin_port = htons(PORT); 
	
	// Bind the socket with the server address 
	if ( bind(sockfd, (const struct sockaddr *)&servaddr, 
			sizeof(servaddr)) < 0 ) 
	{ 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	int len, n; 

	len = sizeof(cliaddr); //len is value/resuslt 
    while(1) // loop until process is killed
     {
	  n = recvfrom(sockfd, &ft8buffer, BUFSIZE, 
				MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
				&len);
     streamID = (int)ft8buffer.stream_ID[3];
	// printf("streamID : %i\n", (int)ft8buffer.stream_ID[3]); 
     if(ft8active[streamID] == 0) //this ft8 stream not yet active
      {
       time_t rawtime;
       struct tm * info ;
       time(&rawtime);
       info = gmtime(&rawtime);
       int seconds = info->tm_sec;
       if(seconds > 0)
         printf("FT8 will start in = %i seconds \r",60-seconds);

       if(seconds != 0)  // check if exact top of minute
         {
          continue;    // we are not at exact top of minute; discard data and wait
         }
       ft8active[streamID] = 1;   // mark this ft8 stream as active
       ft8counter[streamID] = 0;            // zero this counter
       inputcount[streamID] = 0;
       printf("\nSaving FT8 for decode\n");

       t = time(NULL);
       if((gmt = gmtime(&t)) == NULL)
          { fprintf(stderr,"Could not convert time\n"); }
        strftime(date, 12, "%y%m%d_%H%M", gmt);
        sprintf(name[streamID], "%s/FT8/ft8_%i_%i_%d_%s.c2", pathToRAMdisk, streamID, idialfreq,1,date); 

       printf("create raw data FT8 file %s\n",name[streamID]);
       if((fp[streamID] = fopen(name[streamID], "wb")) == NULL)
         { fprintf(stderr,"Could not open file %s \n",name[streamID]);
          return -1;
         }
       double dialfreq1 = dialfreq[streamID];
       fwrite(&dialfreq1, 1, sizeof(dialfreq1), fp[streamID]);
       }
  // when we drop thru to here, we are ready to start recording data

       for(int i=0; i < 1024 && ft8counter[streamID] <= FT8FSIZE; i++)   // go thru input buffer
         {
         inputcount[streamID]++;

         IQval = ft8buffer.theDataSample[i].I_val + (ft8buffer.theDataSample[i].Q_val * I);
         IQval = IQval / 1000000.0;  // experimental; TODO: remove this
         fwrite(&IQval , 1, sizeof(IQval), fp[streamID]);
         ft8counter[streamID]++;

         // if we get another buffer or 2 beyond the last one, we ignore it
         if(ft8counter[streamID] >= FT8FSIZE)   // have we filled output?
           {
           if(ft8counter[streamID] >= (FT8FSIZE+4000))  // have we already done this?
             {
             break;
             }
           IQval = 0.0 + (0.0 * I);
           for(int k = 0; k < 4000; k++)  // pad end of file with zeros
             {
             fwrite(&IQval , 1, sizeof(IQval), fp[streamID]);
             ft8counter[streamID]++;
             }

           ft8active[streamID] = 0;  // mark it inactive
           fclose(fp[streamID]);
         // trigger processing of the ft8 data file

           printf("FT8 decoding...\n");
           char chstr[4];
           sprintf(chstr,"%i",streamID);
           char mycmd[100];
 
           int ret = system(mycmd);

       //    strcpy(mycmd, "./ft8d_del "); 
      //     strcat(mycmd,name[streamID]);
       //    strcat(mycmd," > ");
      //     strcat(mycmd,pathToRAMdisk);
      //     strcat(mycmd, "/FT8/decoded");
       //    strcat(mycmd, chstr);
      //     strcat(mycmd, ".txt &");  // here add '&' to run asynch (but beware of file delete!)
           sprintf(mycmd,"./ft8d_del %s > %s/FT8/decoded%i.txt",name[streamID],pathToRAMdisk,streamID);
           printf("issue command: %s\n",mycmd);
           // Note: this assumes that decoder (ft8d_del) deletes work file when done.
           ret = system(mycmd);
           printf("ft8 decode ran, rc = %i\n",ret);
           sprintf(mycmd,"./upload-to-pskreporter %s %s %s %s/FT8/decoded%d.txt", mycallsign, mygrid,
              myantenna0, pathToRAMdisk, streamID);
           ret = system(mycmd);
           printf("psk upload ran, rc = %i\n",ret);


           }
         }
     


      } // end of while(1) loop

	
	return 0; 
} 

