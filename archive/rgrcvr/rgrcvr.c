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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <string.h>
#include <libconfig.h>
#include "digital_rf.h"
#include <dirent.h>
#include <errno.h>
#include "de_signals.h"
#include <unistd.h>
#include <json.h>
#include <pthread.h>
#include <time.h>
#include <complex.h>
#include <math.h>
#include <fftw3.h>
#include <time.h>
#include <errno.h>
#include "CUnit/CUnit.h"
#include "CUnit/Basic.h"

extern char rconfig(char * arg, char * result, int testThis);
extern int  openConfigFile();

// Definitions for Digital_RF
// length of random number buffer
#define NUM_SUBCHANNELS 1
#define RANDOM_BLOCK_SIZE 4194304 * NUM_SUBCHANNELS
// the last starting index used from buffer
#define N_SAMPLES 1048576
#define WRITE_BLOCK_SIZE 1000000
// set first time to be March 9, 2014   TODO: this to be set by GPSDO or NTP clock
//#define START_TIMESTAMP 1394368230
#define SAMPLE_RATE_NUMERATOR 1000000
#define SAMPLE_RATE_DENOMINATOR 1
#define SUBDIR_CADENCE 10
#define MILLISECS_PER_FILE 1000


///////////////// Digital RF / HDF5 //////////////////////////////////////
// Based on research with old_protocol_N1.c in pihpsdr-master
static	Digital_rf_write_object * DRFdata_object = NULL; /* main object created by init */
//static	Digital_rf_write_object * data_object1 = NULL; /* main object created by init */
static	uint64_t vector_leading_edge_index = 0; /* index of the sample being written starting at zero with the first sample recorded */
static	uint64_t global_start_index; /* start sample (unix time * sample_rate) of first measurement - set below */
static	long hdf_i = 0; 
static  int result = 0;
static  int sampleCounter = 0;
	/*  dataset to write */
static	float data_hdf5[3000];  // needs to be at least 4 x vector lenth + ~72

	/* writing parameters */
static	uint64_t sample_rate_numerator = 48000; // default
static	uint64_t sample_rate_denominator = 1;
static	uint64_t subdir_cadence = 400; /* Number of seconds per subdirectory - typically longer */
static	uint64_t milliseconds_per_file = 4000; /* Each subdirectory will have up to 10 400 ms files */
static	int compression_level = 0; /* low level of compression */
static	int checksum = 0; /* no checksum */
static	int is_complex = 1; /* complex values */
static	int is_continuous = 1; /* continuous data written */
static	int num_subchannels = 1; /* subchannels */
static	int marching_periods = 1; /*  marching periods when writing */
static	char uuid[100] = "DE output";
static  char hdf5File[50];  

static int compression_setting;

static uint64_t theUnixTime = 0;

//char *filename = "/media/odroid/hamsci/raw_data/dat3.dat";  // for testing raw binary output
static  char* sysCommand1;
static  char* sysCommand2;
static	uint64_t vector_length = 1024; /* number of samples written for each call -  */
static  uint64_t vector_sum = 0;

static long buffers_received = 0;  // for counting UDP buffers rec'd in case any dropped in transport

static uint16_t LH_CONF_IN_port;  // port D; DE listens for config request on this port
static uint16_t LH_DATA_IN_port;  // port F; LH listens for spectrum data on this port

static char ringbuffer_path[100];
static char total_hdf5_path[100];
static char firehoseR_path[100];
static char hdf5subdirectory[16];
static long packetCount;
static int recv_port_status = 0;
static int recv_port_status_ft8 = 0;

///////////////////////// FFT //////////////////////////////////////
#define FFT_N 1048576
//#define FFT_N 8192
//#define FFT_N 4096

static int snapcount = 0;
static int fft_busy = 0;  // may need to support max # of possible channels
static char FFToutputPath[75] = "";  

static int numchannels = 1;  // how many channels currently running
static int FFTmemset = 0; // indicates whether FFT plans have been created (TODO: may not be needed)

// The following allocates space for up to 8 FFTs. This could theoretically be done using dynamic
// memory allocation, but that adds much complexity with no significant benefit.
// Note that we use the same memory for both input to and output from fftw. 
struct specPackage
  {
  int channelNo;
  float centerFrequency;
  fftwf_complex FFT_data[FFT_N];
  fftwf_plan p;
  } ;

// set up array for up to 8 FFTs
struct specPackage spectrumPackage[8];
float chfrequency[8];


////////////////////////////////////////////////

// Digital RF - uncomment to enable specific tests
#define TEST_FWRITE
#define TEST_HDF5
#define TEST_HDF5_CHECKSUM
#define TEST_HDF5_CHECKSUM_COMPRESS

//Digital_rf_write_object *data_object = NULL;
//static char path_to_DRF_data[80];

struct sockaddr_in recv_data_addr;    // for data coming from DE


const char *configPath;
static int num_items;  // number of config items found
static char configresult[100];
static char target[30];
static int num_items = 0;
static int snapshotterMode = 0;
static int ringbufferMode = 0;
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

////////////////  FFT Analyze ///////////////////////////////////
// This will be called as a thread for each subchannel when enough samples
// have been received.
void* FFTanalyze(void *arg){  // argument is a struct with all fftwf data
  struct specPackage *threadPkg = (struct specPackage*)arg;
  FILE *fftfp;
  time_t T = time(NULL);
  struct tm tm = *gmtime(&T);  // UTC
  char FFToutputFile[75] = ""; 
  printf("RG: start FFT for subchannel %i, freq %f \n",threadPkg->channelNo, threadPkg->centerFrequency);
  printf("RG: opening fft file: %s, time=",FFToutputFile);
  sprintf(FFToutputFile,"%s/fft%i.csv",FFToutputPath,threadPkg->channelNo);  
  fftfp = fopen(FFToutputFile,"a");
  fprintf(fftfp,"%f,%04d-%02d-%02d %02d:%02d:%02d,",threadPkg->centerFrequency, tm.tm_year+1900, tm.tm_mday, tm.tm_mon+1, tm.tm_hour, tm.tm_min, tm.tm_sec);
  printf("%04d-%02d-%02d %02d:%02d:%02d \n", tm.tm_year+1900, tm.tm_mday, tm.tm_mon+1, tm.tm_hour, tm.tm_min, tm.tm_sec);
//  printf("before fft \n");
//  for(int k=0; k < 10; k++)
 //  printf("%f %f \n",creal(threadPkg->FFT_data[k]),cimag(threadPkg->FFT_data[k]));   
  printf("RG: in thread: exec fft \n");
  fftwf_execute(threadPkg->p); 

 // printf("after fft \n");
 // for(int k=0; k < 10; k++)
 //  printf("%f %f \n",creal(threadPkg->FFT_data[k]),cimag(threadPkg->FFT_data[k]));
  printf("RG: thread FFT analysis complete\n");

  float M = 0;

// The following assumes the signal we are tracking falls within these FFT output bins.
// This works if you use FFT size of 1,048,576 and have slice frequency set to ~ 1 kHz
// below the WWV carrier.
  int lowerbin = 20000 ;
  int upperbin = 22000 ;

//  int lowerbin = 0;
//  int upperbin = 8191 ;

  float maxval = 0;
  long maxbin = 0;
  float maxvalT = 0.0;
  long maxbinT = 0;

// find max signal bin
  for(int i=0; i < FFT_N; i++)  // find maximum bin in entire histogram
   {
    M = sqrt( creal(threadPkg->FFT_data[i])*creal(threadPkg->FFT_data[i])+cimag(threadPkg->FFT_data[i])*cimag(threadPkg->FFT_data[i]) );
    if(M>maxvalT)
      {
      maxvalT = M;
      maxbinT = i ;
      }
   }

  if(maxbinT < 100)
    maxbinT = 100;   // if maxbin < 100, then avoid having pointer go outside array

  printf("RG: output FFT results\n");

#ifdef FULLOUTPUT

// this section is for outputting entire histogram in correct order
  for(int i=FFT_N/2;i >=0;i--)  // frequencies above center freq. ignoring DC
   {
    M = sqrt( creal(threadPkg->FFT_data[i])*creal(threadPkg->FFT_data[i])+cimag(threadPkg->FFT_data[i])*cimag(threadPkg->FFT_data[i]) );
    if(M > maxval)
     {
      maxbin = i;
      maxval = M;
     }
     fprintf(fftfp,"%15.10f,",M);
   }

  for(int i=FFT_N;i >=FFT_N/2;i--)  // frequencies above center freq. ignoring DC
   {
    M = sqrt( creal(threadPkg->FFT_data[i])*creal(threadPkg->FFT_data[i])+cimag(threadPkg->FFT_data[i])*cimag(threadPkg->FFT_data[i]) );
    if(M > maxval)
     {
      maxbin = i;
      maxval = M;
     }
     fprintf(fftfp,"%15.10f,",M);
   }

#else

// this section is for outputting +/- 5 hz around center freq with FFT_N=1,048,576

  for(int i=maxbinT-100;i < maxbinT+100;i++)  
   {
    M = sqrt( creal(threadPkg->FFT_data[i])*creal(threadPkg->FFT_data[i])+cimag(threadPkg->FFT_data[i])*cimag(threadPkg->FFT_data[i]) );
    if(M > maxval)
     {
      maxbin = i;
      maxval = M;
     }

     fprintf(fftfp,"%15.10f,",M);

   }

     if(maxbin != maxbinT)
        printf("** WARNING - overall maxbin (%li) outside of focus area max (%li)\n",maxbinT, maxbin);

#endif

  printf("RG: maxbin = %li, maxval = %f\n",maxbinT,maxvalT);
  fprintf(fftfp,"%li,%f,\n",maxbin,maxval);
  fclose(fftfp);

}

/////////////////////////////////////////////////////////////////////////////////////
// the following function parses input to get start and end date-times for data upload
int getDataDates(char *input, char* startpoint, char* endpoint)
 {
 // printf("input to extract from= '%s'\n", input);
  int theLen = strlen(input);
  printf("RG: len = %i\n",theLen);
  if(theLen < 180)
   {
   return(0);  // this is not a DR command
   }
  printf("RG: last char = %c\n",input[theLen - 1]);
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

///////////////// create file name for upload data //////////////////
const char * buildFileName(char * node, char * grid){

 static char thefilename[100] = "";
 time_t t= time(NULL);
 struct tm *tm = localtime(&t);
 char s[64];
 assert(strftime(s, sizeof(s), "%FT%H%M%SZ", tm));
 printf("RG: computed time=%s\n",s);
 strcpy(thefilename, s);
 strcat(thefilename, "_");
 printf("RG: the node = %s\n", node);
 strcat(thefilename, node);
 strcat(thefilename, "_T1_");
 strcat(thefilename, grid);
 strcat(thefilename, "_DRF.tar");
 printf("RG: filename: %s\n", thefilename);
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
  printf("RG: ringbuffer_path=%s\n",ringbuffer_path);
// store the list of file names in the RAMdisk
  sprintf(fcommand,"drf ls %s -r -s %s -e %s > %s/dataFileList", ringbuffer_path, startDT, endDT, pathToRAMdisk);
  printf("RG: ** drf fcommand1='%s'\n",fcommand);
  int retcode = system(fcommand);  // execute the command
  printf("RG: drf return code=%i\n",retcode);
// build arguments for a script that will tar the files, saving
// the compressed (tar) file in same location as the ringbuffer
  int num_items = rconfig("node",theNode,0);
  num_items = rconfig("grid",theGrid,0);
  strcpy(fn, (char *)buildFileName(theNode, theGrid));
  sprintf(fcommand,"./filecompress.sh %s  %s/dataFileList %s ", ringbuffer_path, pathToRAMdisk, fn);
  printf("RG: ** file compress command=%s\n",fcommand);
  retcode = retcode + system(fcommand); // execute the command
  printf("RG: compress retcode = %i\n",retcode);
  return(retcode);
 }



///////////////////////////////////////////////////////////////////////////
///////// Thread for uploading firehoseR data to Central Control //////////
void firehose_uploader(void *threadid) {

  char sys_command[200];
  printf("RG: firehoseR uploader thread starting\n");
  sleep(20);
  while(1)
   {
   if (firehoseUploadActive == 0)  // firehoseR upload halted
     {
     printf("RG: ------ FIREHOSE UPLOAD SHUTTING DOWN -------\n");
     return;
     }
   printf("RG: ------FIREHOSE UPLOAD-----------\n");

  sprintf(sys_command,"./firehose_xfer_auto.sh %s %s %s", data_path,temp_path,the_node);
  printf("RG: Uploader - executing command: %s \n",sys_command); 
  int r = system(sys_command); 
  printf("RG: System command retcode=%i\n",r);

   sleep(10);

   }

  return;
}

/*
///////////////////// open config file ///////////////
int openConfigFile()
{
  printf("test - config init\n");
  config_init(&cfg);

  // Read the file. If there is an error, report it and exit. 

// The only thing we use this config file for is to get the path to the
// python config file. Seems like a kludge, but allows flexibility in
// system directory structure.

  printf("test - read config file\n");
  if(! config_read_file(&cfg, "/home/odroid/projects/TangerineSDR-notes/mainctl/main.cfg"))
  {
    fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
            config_error_line(&cfg), config_error_text(&cfg));
    puts("ERROR - there is a problem with main.cfg configuration file");
    config_destroy(&cfg);
    return(EXIT_FAILURE);
  }
  printf("test - look up config path\n");
  if(config_lookup_string(&cfg, "config_path", &configPath))
    printf("Setting config file path to: %s\n\n", configPath);
  else
    fprintf(stderr, "No 'config_path' setting in configuration file main.cfg.\n");
    return(EXIT_FAILURE);
  printf("test - config path=%s\n",configPath);
  return(0);
} 

*/

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
int main() {

  int sock;
  struct sockaddr_in si_LH;
  struct VITAdataBuf myDataBuf;

  openConfigFile();  // connect with config files
  // Determine control & data ports
  num_items = rconfig("configport0",configresult,0);
  if(num_items == 0)
    {
    printf("RG: ERROR - configport0 setting not found in config.ini");
    }
  else
    {
    printf("RG:  CONFIG RESULT = '%s'\n",configresult);
 //   printf("len =%lu\n",strlen(result));
    LH_CONF_IN_port = atoi(configresult);
    // In this version, we always use channel 0 for ringbuffer type data
    num_items = rconfig("dataport0",configresult,0);
    LH_DATA_IN_port = atoi(configresult);
    printf("RG: Will receive data into port %i\n",LH_DATA_IN_port);
    }

  // How many subchannels is the user expecting?
  num_items = rconfig("numchannels",configresult,0);
  if(num_items == 0)
    {
    printf("RG: ERROR - numchannels setting not found in config.ini");
    }
  else
    {
    printf("RG:  CONFIG RESULT = '%s'\n",configresult);
 //   printf("len =%lu\n",strlen(result));
    // Later, if DE sends a different# subchannels in data, error message
    numchannels = atoi(configresult);
    }

  // Is ringbuffer mode set to "On" in config?
  // Get the config items needed for ringbuffer mode
  num_items = rconfig("drf_compression",configresult,0);
  if(num_items == 0)
    {
    printf("RG: ERROR - drf_compression config setting not found in config.ini\n");
    }
  else
    {
    printf("RG: drf_compression CONFIG RESULT = '%s'\n",configresult);
    compression_setting = atoi(configresult);
    printf("RG: compression level set to %i\n",compression_setting);
    } 

  num_items = rconfig("ringbuffer_mode",configresult,0);
  if(num_items == 0)
    {
    printf("RG: ERROR - ringbuffer_mode setting not found in config.ini");
    }
  else
    {
    printf("RG: CONFIG RESULT = '%s'\n",configresult);

    if ( memcmp(&configresult,"On",2) == 0)
      {
      ringbufferMode = 1;
      // get ringbuffer path since ringbuffer_mode is on
      num_items = rconfig("ringbuffer_path",configresult,0);
      if(num_items == 0)
        {
        printf("RG: ERROR - ringbuffer_path setting not found in config.ini");
        }
      else
        {
        printf("RG:  CONFIG RESULT = '%s'\n",configresult);
        strcpy(ringbuffer_path, configresult);
        }
   
      }
    else
      {
      ringbufferMode = 0;
      }
    }

  // Is snapshotter mode set to "On" in config?
  num_items = rconfig("snapshotter_mode",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - snapshotter_mode setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT = '%s'\n",configresult);

    if ( memcmp(&configresult,"On",2) == 0)
      {
      snapshotterMode = 1;


       num_items = rconfig("fftoutput_path",configresult,0);
       if(num_items > 0)
        {
         strcpy(FFToutputPath,configresult);
       
        printf("RG: FFT output path set to %s\n",FFToutputPath);
        }
       else
        {
        printf("RG: Snapshotter mode specified but no data output path in config.ini\n");
        exit(-1); 
        }





      }
    else
      {
      snapshotterMode = 0;
      }
    }

  // Is firehoseR mode set to "On" in config?
  num_items = rconfig("firehoser_mode",configresult,0);
  if(num_items == 0)
    {
    printf("RG: ERROR - firehoser_mode setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT = '%s'\n",configresult);

    if ( memcmp(&configresult,"On",2) == 0)
      {
      // config file specifies firehoser mode; if ringbuffer specified also, error
      if(ringbufferMode == 1)
        {
        printf("* * * ERROR * * * CANNOT RUN RINGBUFFER AND FIREHOSE-R MODE AT SAME TIME");
        exit(-1);
        }
      firehoseRMode = 1;
      // get firehose R path, since firehoser mode is on
      num_items = rconfig("firehoser_path",configresult,0);
      if(num_items == 0)
        {
        printf("RG: ERROR - firehoser_path setting not found in config.ini");
        }
     else
        {
        printf("RG:  CONFIG RESULT = '%s'\n",configresult);
        strcpy(firehoseR_path, configresult);
        }

      num_items = rconfig("drf_compression",configresult,0);
      if(num_items == 0)
        {
        printf("RG: ERROR - drf_compression config setting not found in config.ini\n");
        }
      else
        {
        printf("RG: drf_compression CONFIG RESULT = '%s'\n",configresult);
        compression_setting = atoi(configresult);
        printf("RG: compression level set to %i\n",compression_setting);
        } 

      }
    else
      {
      firehoseRMode = 0;
      }
    }

      for (int i = 0; i < numchannels; i++) 
        {
        if(snapshotterMode == 1)  // is snapshotter mode set on?
          {  // if so, set up FFT plan for each subchannel
          printf("RG: FFTW create plans %i\n",i);
          spectrumPackage[i].channelNo = i;
          spectrumPackage[i].p = fftwf_plan_dft_1d(FFT_N, spectrumPackage[i].FFT_data, spectrumPackage[i].FFT_data, FFTW_FORWARD, FFTW_ESTIMATE);
          printf("RG: plans created for fft %i\n",i);
          }
        // now get the frequency for each channel
        char channelSelect[12];
        sprintf(channelSelect,"f%i",i);
        num_items = rconfig(channelSelect,configresult,0);
        if(num_items == 0)
          {
          printf("RG: ERROR - frequency config setting %s not found in config.ini\n", channelSelect);
          }
        else
          {
          printf("RG: channel %i frequency CONFIG RESULT = '%s'\n",i,configresult);
          chfrequency[i]= atof(configresult);
          } 
        }

  // Set up socket to receive data packets from DE
  if ( (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
    perror("socket");
    exit(-1);
    }

  memset((char *) &si_LH, 0, sizeof(si_LH));
  si_LH.sin_family = AF_INET;
  si_LH.sin_port = htons(LH_DATA_IN_port);
  si_LH.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock, (struct sockaddr *)&si_LH, sizeof(si_LH)) < 0)
    {
    perror("bind");
    exit(-1);
    }

  buffers_received = 0; // initialize this
  printf("RG: ringbufferMode=%i, snapshotterMode=%i,firehoseRMode=%i\n",
    ringbufferMode,snapshotterMode,firehoseRMode);

while(1)
 {
 // start listening loop here
  int slen = sizeof(si_LH);
  int recv_len;
  if( (recv_len = recvfrom(sock, &myDataBuf, sizeof(myDataBuf), 0, 
      (struct sockaddr *) &si_LH, &slen)) < 0)
    {
    perror("recvfrom");
    exit(-1);
    }
   
//  printf(" databuf rec'd buflen= %i, packetsize=%i, streamID= %s, time=%d, count=%ld \n", 
  //  recv_len, myDataBuf.VITA_packetsize, myDataBuf.stream_ID, myDataBuf.time_stamp,
   // myDataBuf.sample_count);
  
  int channelPtr;
  
  /////////////////////////////////////////////////////////
  ////  start of handling incoming I/Q data //////////////

  /////////////////// Handling RG (ringbuffer - type data ///////////////

  //printf("Handle incoming buffer\n");
 // TODO: this needs to detect the slightly different header bytes in "VITA-T"

 // if(buf_ptr1->VITA_hdr1[0] == 0x1c && buf_ptr1->stream_ID[0] == 0x52 && buf_ptr1->stream_ID[1] == 0x47)

  if(myDataBuf.VITA_hdr1[0] = 0x1c && myDataBuf.stream_ID[0] == 0x52 && myDataBuf.stream_ID[1] == 0x47)
   {
   //printf("buffer# %li\n",buffers_received);
   buffers_received++;  // bump this to reflect how many received so far
   int bufferChannels = myDataBuf.stream_ID[2];  // number of channels embedded in payload
// Note above: could use either streamID byte [2] or byte [3] for this
   if (recv_len < 8000)  // discard this buffer for now 
	{
	 fprintf(stderr, "RG: Buffer is < 8000 bytes(starts with %x %x;  * * * IGNORE * * *\n",
       myDataBuf.VITA_hdr1[0], myDataBuf.VITA_hdr1[1]      );
	 continue; 
	}

   if(snapshotterMode && !fft_busy) // here we collect data until input matrix full, then start FFT thread
    { 
    // Note: following math intentionally discards the remainder of the division
    int numSamples = 1024 / numchannels;  // how many IQ pair-groups in this buffer  
    for(int j=0; j < numSamples; j=j+numchannels)
      {
          for (int i = 0; i < numchannels; i++)  
           {
          //  printf("value %i %f ",j, buf_ptr->theDataSample[j+i].I_val);
            spectrumPackage[i].FFT_data[snapcount] = myDataBuf.theDataSample[j+i].I_val + myDataBuf.theDataSample[j+i].Q_val * I; ;
           }
         snapcount++;
      }    

    if(snapcount >= FFT_N)
      {
      fft_busy = 1;  // block further use of this until the thread(s) complete
      printf("RG: **prep to start FFT thread ***, snapcount=%i\n",snapcount);\

      pthread_t tid[8];
      for(int i=0; i <= numchannels-1; i++)
       {
       printf(" \n");
       printf("RG: start thread %i\n",i);
       spectrumPackage[i].centerFrequency = chfrequency[i];
// TODO: possibly all threads should be started here, and then all joins done
       pthread_create(&(tid[i]), NULL, FFTanalyze, &spectrumPackage[i]);    

       printf("RG: join thread %i\n",i);

       pthread_join(tid[i],NULL);

       printf("RG: DFT # %i done\n", i);
       }
 
      snapcount = 0;
      fft_busy = 0;
      }
// we are done with snapshotter handing of this buffer; if user doesn't want ringbuffer also, free memory & exit
    if(!ringbufferMode)
      {
      continue;
      } // if we fall thru the above if stmt, it means user wants both snapshotter & ringbuffer modes
    } // end of handling FFT


  ////////////////////////////////////////////////////////////////////////////////////
  // handle I/Q buffers coming in for storage to Digital RF /////////////////////////

    {
    packetCount = (long) myDataBuf.sample_count;
  //  printf("bufcount = %ld\n", buf_ptr1->sample_count);
/*
    if(myDataBuf.channelCount != numchannels)
      printf("**** WARNING - subchannel count in data buffer (%i) differs from number of channels in config setting (%i) ***\n", myDataBuf.channelCount, numchannels);
*/
    int noOfChannels = numchannels;
    int sampleCount = 1024 / numchannels;
 //   printf("Channel count = %i\n",buf_ptr->channelCount);
  // local variables  for Digital RF  
    uint64_t vector_leading_edge_index = 0;
    uint64_t global_start_sample = 0;

// if bufCount is zero, it is first data packet; create the Digital RF file;
// we also check buffers_received, so we can start recording even if we missed
// one or more buffers at start-up.

  global_start_sample = myDataBuf.time_stamp * (long double)sample_rate_numerator /  
                 SAMPLE_RATE_DENOMINATOR;

  // We do all this upon receiving the first packet of a new data collection session
  // If we missed buffer zero (it is UDP, after all), we consider it "first" if buffers_received=1
  if((packetCount == 0 || buffers_received == 1) && DRFdata_object == NULL) 
    {
    char cleanup[100]="";

/*
    if(firehoseRMode)  // decide where we will store the DRF (hdf5) files
      {
      strcpy(total_hdf5_path,firehoseR_path);
      sprintf(cleanup,"rm %s/firehose/drf_properties.h5",firehoseR_path);
      }
    else
      {
      strcpy(total_hdf5_path,ringbuffer_path);
      sprintf(cleanup,"rm %s/TangerineData/drf_properties.h5",ringbuffer_path);
      }
*/
    // removed old drf properties file so DRF can record current config info
    sprintf(cleanup,"rm %s/TangerineData/drf_properties.h5",ringbuffer_path);
    strcpy(total_hdf5_path,ringbuffer_path);
    sprintf(cleanup,"rm %s/drf_properties.h5",firehoseR_path); 
   
    printf("RG: deleting old propeties file: %s\n",cleanup);
    int retcode = system(cleanup);
    fprintf(stderr,"RG: Create HDF5 file group, start time: %ld \n",global_start_sample);
    fprintf(stderr,"RG: data rate = %li\n",sample_rate_numerator);
    vector_leading_edge_index=0;
    vector_sum = 0;
    hdf_i= 0;

  //  strcat(total_hdf5_path,"/");
  //  strcat(total_hdf5_path, hdf5subdirectory);
    printf("RG: Using compression level %i; Storing to: %s\n",compression_setting, total_hdf5_path);
    printf("RG: subdir cadence=%li,msec=%li, rate=%li,chnls=%i\n",subdir_cadence,milliseconds_per_file, sample_rate_numerator,bufferChannels);

    // Create the new DRF directory structure & properties file
    DRFdata_object = digital_rf_create_write_hdf5(total_hdf5_path, H5T_NATIVE_FLOAT, subdir_cadence,
      milliseconds_per_file, global_start_sample, sample_rate_numerator, SAMPLE_RATE_DENOMINATOR,
     "TangerineSDR", compression_setting, 0, 1, bufferChannels, 1, 1);
      }  // end of processing first received IQ packet

// here we write out DRF

    vector_sum = vector_leading_edge_index + hdf_i*sampleCount; 

    if(DRFdata_object != NULL)  // make sure there is an open DRF file
	  {
      result = digital_rf_write_hdf5(DRFdata_object, vector_sum, myDataBuf.theDataSample,sampleCount) ;
	//  fprintf(stderr,"DRF write result = %d, vector_sum = %ld \n",result, vector_sum);
	  }
    hdf_i++;  // increment count of hdf buffers processed
    }
  }


 }
  
  return(0);  // end of handling incoming I/Q data

}  // end of main










