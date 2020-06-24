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
* libuv - for asynchronous processing
  hdf5
  Digital_RF
*/

#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
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
#include "CUnit/CUnit.h"
#include "CUnit/Basic.h"
#include "discovered.h"

extern DISCOVERED UDPdiscover();
static uint16_t LH_port;   // port A, used on LH (local host) for sending to DE, and will listen on
static uint16_t DE_port;   // port B, that DE will listen on
static char DE_IP[16];

static uint16_t LH_CONF_IN_port;  // port C, receives ACK or NAK from config request
static uint16_t LH_CONF_OUT_port; // for sending (outbound) config request to DE
static uint16_t DE_CONF_IN_port;  // port D; DE listens for config request on this port
static uint16_t LH_DATA_IN_port;  // port F; LH listens for spectrum data on this port
static uint16_t DE_DATA_IN_port;  // port E; DE listens for xmit data on this port
static uint16_t LH_DATA_OUT_port; // for sending (outbound) data (e.g., mic audio) to DE

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
//static  char hdf5File1[50] = "/tmp/RAM_disk/ch4_7";
static uint64_t theUnixTime = 0;
//static  char hdf5File[50] = "/tmp/ramdisk";
//char *filename = "/media/odroid/hamsci/raw_data/dat3.dat";  // for testing raw binary output
static  char* sysCommand1;
static  char* sysCommand2;
static	uint64_t vector_length = 1024; /* number of samples written for each call -  */
static  uint64_t vector_sum = 0;

static long buffers_received = 0;  // for counting UDP buffers rec'd in case any dropped in transport

static char ringbuffer_path[80];
static char total_hdf5_path[100];
static char hdf5subdirectory[16];
static long packetCount;
static int recv_port_status = 0;

///////////////////////// FFT //////////////////////////////////////
#define FFT_N 1048576
//#define FFT_N 8192

static int snapcount = 0;
static int fft_busy = 0;  // may need to support max # of possible channels
static char FFToutputPath[75] = "/mnt/RAM_disk/";  // TODO: must come from config file
static int numchannels = 1;  // how many channels currently running
static int FFTmemset = 0; // indicates whether FFT plans have been created (TODO: may not be needed)

//  structure to contain data to pass to FFTanalyze thread
// To be used only in the case of dynamic memory management for the FFT
/*
struct specPackage
  {
  int channelNo;
  float centerFrequency;
  fftw_complex *spectrum_in;
  fftw_complex *FFTout;
  fftw_plan p;
  } ;
*/

// The following purportedly can be done using dynamic mamory allocation utiltes
// provided with FFTW package; however, this is very tricky to get to work without
// creating memory leaks. So, we take brute force approach here instead.
struct specPackage
  {
  int channelNo;
  float centerFrequency;
  fftwf_complex spectrum_in[FFT_N];
  fftwf_complex FFTout[FFT_N];
  fftwf_plan p;
  } ;

// set up array for up to 8 FFTs
struct specPackage spectrumPackage[8];


////////////////////////////////////////////////

// uncomment to enable specific tests
#define TEST_FWRITE
#define TEST_HDF5
#define TEST_HDF5_CHECKSUM
#define TEST_HDF5_CHECKSUM_COMPRESS

//Digital_rf_write_object *data_object = NULL;
//static char path_to_DRF_data[80];

// Variables for asynchtonous processing using libuv library
uv_loop_t *loop;
static uv_tcp_t* DEsocket; // socket to be used across multiple functions
static uv_stream_t* DEsockethandle;

// the following probably not necessary  (TODO)
static uv_tcp_t* websocket; // socket to be used for comm to web server
//static uv_stream_t* websockethandle;
static uv_udp_t send_socket;
static uv_udp_t data_socket;
static uv_udp_t send_config_socket;
static uv_stream_t* webStream;

struct sockaddr_in send_addr;         // used for sending commands to DE
struct sockaddr_in recv_addr;         // for command replies from DE (ACK, NAK)
struct sockaddr_in send_config_addr;  //used for sending config req (CH) to DE
struct sockaddr_in recv_config_addr;  // for config replies from DE
struct sockaddr_in recv_data_addr;    // for data coming from DE

static long packetCount;

// variables for FT8 reception
char date[12];
char name[8][64];
time_t t;
struct tm *gmt;
FILE *fp[8];

// Set up memory to handle data traffic passing via libuv asynchronous calls
static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

static void alloc_config_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

static void alloc_data_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

const char *configPath;
static int num_items;  // number of config items found
static char configresult[100];
static char target[30];
static int num_items = 0;
static int snapshotterMode = 0;
static int ringbufferMode = 0;
static int firehoseMode = 0;
static char pathToRAMdisk[50];
static int uploadInProgress = 0;
// for communications to Central Host
static char central_host[100];
static uint16_t central_port;
// the configuration file
static config_t cfg;
static config_setting_t *setting;


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
  return(1);
   }
  }
  free(cp);
  return(0);
}

/////////////////////////////////////////////////////////////////
//  Callback after packet sent by UDP
///////////////////////////////////////////////////////////////
void on_UDP_send(uv_udp_send_t* req, int status)
{
  printf("UDP send complete, status: %d\n", status);
 // free(req);   // not sure if this is right; may crash
  //puts("free memory completed");
  return;
}

//////////////////////////////////////////////////////////
// callback routine for after write to webcontrol is complete
////////////////////////////////////////////////////////
void web_write_complete(uv_write_t *req, int status) {
  printf("webctl write status = %d\n", (int)status);
  if (status == -1) {
    fprintf(stderr, "Write error!\n");
  }
  char *base = (char*) req->data;
  puts("free req");
  free(req);
}

/////////////////////////////////////////////////////////////
// callback for when write to DE is complete
///////////////////////////////////////////////////////////
void DE_write_cb(uv_write_t *req, int status) {
  if (status == -1) {
    fprintf(stderr, "Write error!\n");
  }
  char *base = (char*) req->data;
  puts("free req");
  free(req);
}

//////////////////////////////////////////////////////////////
// callback for when TCP data is received from DE
/////////////////////////////////////////////////////////////
void handleDEdata(uv_stream_t* client, ssize_t nread, const uv_buf_t* DEbuf) {
  if(nread <=0)
	return;
  puts("got data from DE");
  char reply[80];
  memset(&reply, 0, sizeof(reply));
  strncpy(reply,DEbuf->base, nread);
  fprintf(stderr,"DE sent %zd: bytes:\n", nread);
  puts(reply); 

/////////////////////////////////////////////////////////////////
//////////  forward a status message from DE to webcontrol
//////////////////////////////////////////////////////////////////
  puts("process_command triggered; set up uv_write_t");
  uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
  puts("set up write_req");
  uv_buf_t a[]={{.base="OK", .len=2},{.base="\n",.len=1}};
  puts("forward status to webcontrol");
  uv_write(write_req, (uv_stream_t*) webStream, a, 2, web_write_complete);
  puts("free the DE buffer");
  free(DEbuf->base);    
}

//////////////////////////////////////////////////////////////////////////
///////////////////// thread for running FFT on one structure ////////////
//////////////////////////////////////////////////////////////////////////
void FFTanalyze(void *args){  // argument is a struct with all fftwf data
  struct specPackage *threadPkg = args;
  FILE *fftfp;
  time_t T = time(NULL);
  struct tm tm = *gmtime(&T);  // UTC
  char FFToutputFile[75] = ""; 
  printf("opening fft file: %s\n",FFToutputFile);
  sprintf(FFToutputFile,"%sfft%i.csv",FFToutputPath,threadPkg->channelNo);  
  fftfp = fopen(FFToutputFile,"a");
  fprintf(fftfp,"%f,%04d-%02d-%02d %02d:%02d:%02d,",threadPkg->centerFrequency, tm.tm_year+1900, tm.tm_mday, tm.tm_mon+1, tm.tm_hour, tm.tm_min, tm.tm_sec);
   // printf("in thread: exec fft \n");
    fftwf_execute(threadPkg->p);
   // }
 printf("thread FFT analysis complete\n");

 float M = 0;

//  TODO: code below based on empirical approach (Red Pitaya). Must be fixed for Tangerine
// assumes 1,048,572 bins per FFT
  int lowerbin = 20000 ;
  int upperbin = 22000 ;

  float maxvalT = 0.0;
  long maxbinT = 0;

/*
// find max signal bin
  for(int i=lowerbin; i < F; i++)  // find maximum bin in positive frequency sectino
   {
       M = sqrt( creal(threadPkg->FFTout[i])*creal(threadPkg->FFTout[i])+cimag(threadPkg->FFTout[i])*cimag(threadPkg->FFTout[i]) );
    if(M>maxvalT)
      {
      maxvalT = M;
      maxbinT = i ;
      }
   }
*/

  printf("output FFT results\n");

  for(int i=lowerbin;i < upperbin;i++)  // frequencies above center freq. ignoring DC
   {
    M = sqrt( creal(threadPkg->FFTout[i])*creal(threadPkg->FFTout[i])+cimag(threadPkg->FFTout[i])*cimag(threadPkg->FFTout[i]) );
    if(M > maxvalT)
     {
      maxbinT = i;
      maxvalT = M;
     }

       fprintf(fftfp,"%15.10f,",M);
   }



/*  the following puts out the entire FFTout array
  for(int i=1;i < FFT_N/2;i++)  // frequencies above center freq. ignoring DC
   {
    M = sqrt( creal(threadPkg->FFTout[i])*creal(threadPkg->FFTout[i])+cimag(threadPkg->FFTout[i])*cimag(threadPkg->FFTout[i]) );

       fprintf(fftfp,"%15.10f,",M);
   }
  for(int i=FFT_N-1;i > FFT_N/2;i--)  // frequencies below center in reverse order
   {
    M = sqrt( creal(threadPkg->FFTout[i])*creal(threadPkg->FFTout[i])+cimag(threadPkg->FFTout[i])*cimag(threadPkg->FFTout[i]) );

       fprintf(fftfp,"%15.10f,",M);
   }
*/


  printf("maxbin = %li, maxval = %f\n",maxbinT,maxvalT);
  fprintf(fftfp,"%li,%f,\n",maxbinT,maxvalT);
  fclose(fftfp);

/*
// code to display freq. histogram (needs fixed)
  float scale = 25.0 / maxval;
  char display[25][100];
  int avg_ct = 0;
  float mag = 0;
  char space[1] = " ";
  char aster[1] = "*";
  // clear array
  for (int y=0; y < 24; y++) {
   for (int x=0; x < 100; x++) {
    display[y][x]=32;
    }
   }
  for(int bin=FFT_N; bin > FFT_N/2; bin--)
   {
   mag = mag + FFTout[bin];
   avg_ct ++;
   if(avg_ct == 5)
     {
     mag = 25 * mag / (avg_ct * 0.3);
     for(int i = 0; i < mag; i++)
       display[i][bin]=42;
     avg_ct = 0;
     mag = 0.0;
     }
   }

   for(int y = 24; y > 0; y--)
    {
    for(int x=0;x<100;x++)
     printf("%c",display[x][y]);
    printf("\n");
    }
*/

}


/////////////////////////////////////////////////////////////
//  Callback for when UDP I/Q data packets received from DE 
//  A separate callback handles incoming ACK data.
/////////////////////////////////////////////////////////////
void on_UDP_data_read(uv_udp_t * recv_handle, ssize_t nread, const uv_buf_t * buf,
		const struct sockaddr * addr, unsigned flags)
  {
  int channelPtr;
  if(nread == 0 )
    { 
    //  puts("received UDP zero");
      free(buf->base);
	  return;
    }
 // printf("UDP I/Q data recvd, bytes = %ld\n", nread);

  DATABUF *buf_ptr;
  buf_ptr = (DATABUF *)malloc(sizeof(DATABUF));  // allocate memory for working buf
  memcpy(buf_ptr, buf->base,nread);    // get data from UDP buffer
 // printf("DE BUFTYPE = %s \n",buf_ptr->bufType);


////  start of handling incoming I/Q data //////////////

  if(strncmp(buf_ptr->bufType, "FT" ,2) ==0)  // this is a buffer of FT8
    {
       double dialfreq = buf_ptr->centerFreq;
       channelPtr = buf_ptr-> channelNo;
       printf("FT8 data, f = %f, buf# = %ld \n",buf_ptr->centerFreq, buf_ptr->dval.bufCount);
       if(buf_ptr->dval.bufCount == 0 )   // this is the first buffer of the minute
       {
        t = time(NULL);

        if((gmt = gmtime(&t)) == NULL)
          { fprintf(stderr,"Could not convert time\n"); }
        strftime(date, 12, "%y%m%d_%H%M", gmt);
        sprintf(name[channelPtr], "/mnt/RAM_disk/FT8/ft8_%d_%f_%d_%s.c2", 1, buf_ptr->centerFreq,1,date);
       if((fp[channelPtr] = fopen(name[channelPtr], "wb")) == NULL)
        { fprintf(stderr,"Could not open file %s \n",name[channelPtr]);
		  free(buf->base);  // always release memory before exiting this callback
		  free(buf_ptr);
          return;
        }
       fwrite(&dialfreq, 1, 8, fp[channelPtr]);
      }
      fwrite(buf_ptr->theDataSample, 1, 8000, fp[channelPtr]);
      if (buf_ptr->dval.bufCount == 239 )   // was this the last buffer?
        {
        fclose(fp[channelPtr]);
        char chstr[2];
        sprintf(chstr,"%d",channelPtr);
        char mycmd[100];
        strcpy(mycmd, "./ft8d ");
        strcat(mycmd,name[channelPtr]);
        strcat(mycmd, "  > /mnt/RAM_disk/FT8/decoded");
        strcat(mycmd, chstr);
        strcat(mycmd, ".txt &");
        printf("the command: %s\n",mycmd);
        int ret = system(mycmd);
        puts("ft8 decode ran");

        }
	free(buf->base);  // always release memory before exiting this callback
    free (buf_ptr);
	return; 
    }  // end of code for handling incoming FT8 data

  if(strncmp(buf_ptr->bufType, "RG" ,2) ==0)  // this is a buffer of ringbuffer I/Q data
  {
   buffers_received++;
  //fprintf(stderr,"received UDP packet# %zd\n",packetCount);
 //  fprintf(stderr,"nread = %zd\n", nread);
 //  fprintf(stderr,"Buffer starts with: %02x %02X \n",buf->base[0], buf->base[1]);
   if (nread < 8000)  // discard this buffer for now 
	{
	fprintf(stderr, "Buffer is < 8000 bytes;  * * * IGNORE * * *\n");
	 free(buf->base);  // always release memory before exiting this callback
     free (buf_ptr);
	 return; 
	}

   if(snapshotterMode && !fft_busy) // here we collect data until input matrix full, then start FFT thread
    { 

    int numSamples = 1024 / buf_ptr->channelCount;  // how many IQ pairs in this buffer
    //printf("Num samples = %i\n",numSamples);
    
    for(int j=0; j < numSamples; j=j+buf_ptr->channelCount)  // iteration based on # channels running  
      {

          for (int i = 0; i < buf_ptr->channelCount; i++)  
           {
    //  printf("%i   %i   %i \n",j,i,snapcount);
            spectrumPackage[i].spectrum_in[snapcount] = buf_ptr->theDataSample[j+i].I_val + buf_ptr->theDataSample[j+i].Q_val * I; ;

           }
         snapcount++;
      }    

    if(snapcount >= FFT_N)
      {
      fft_busy = 1;  // block further use of this until the thread(s) complete
      printf("**prep to start FFT thread ***\n");\

      uv_thread_t analyzethread[8];

      for(int i=0; i < buf_ptr->channelCount; i++)
       {

       printf(" \n");
       printf("start thread %i\n",i);
       spectrumPackage[i].centerFrequency = buf_ptr->centerFreq;
       uv_thread_create(&analyzethread[i], FFTanalyze, &spectrumPackage[i]);    
       printf("join thread %i\n",i);
       uv_thread_join(&analyzethread[i]);

       printf("DFT # %i done\n", i);
       }
 
      snapcount = 0;
      fft_busy = 0;
      }

    }



////////////////////////////////////////////////////////////////////////////////////
// handle I/Q buffers coming in for storage to Digital RF

    {

    packetCount = (long) buf_ptr->dval.bufCount;
 //   printf("bufcount = %ld\n", packetCount);
    int noOfChannels = buf_ptr->channelCount;
    int sampleCount = 1024 / noOfChannels;
 //   printf("Channel count = %i\n",buf_ptr->channelCount);

  /* local variables  for Digital RF */
    uint64_t vector_leading_edge_index = 0;
    uint64_t global_start_sample = 0;

// if bufCount is zero, it is first data packet; create the Digital RF file
// we also check buffers_received, so we can start recording even if we missed
// one or more buffers at start-up.

  global_start_sample = buf_ptr->timeStamp * (long double)sample_rate_numerator /  
                 SAMPLE_RATE_DENOMINATOR;

  if((packetCount == 0 || buffers_received == 1) && DRFdata_object == NULL) {
    char cleanup[100]="";
    sprintf(cleanup,"rm %s/TangerineData/drf_properties.h5",ringbuffer_path);
    printf("deleting old propeties file: %s\n",cleanup);
    int retcode = system(cleanup);
    fprintf(stderr,"Create HDF5 file group, start time: %ld \n",global_start_sample);
    fprintf(stderr,"data rate = %li\n",sample_rate_numerator);
    vector_leading_edge_index=0;
    vector_sum = 0;
    hdf_i= 0;

    strcpy(total_hdf5_path,ringbuffer_path);
    strcat(total_hdf5_path,"/");
    strcat(total_hdf5_path, hdf5subdirectory);
    printf("M: Storing to: %s\n",total_hdf5_path);

    DRFdata_object = digital_rf_create_write_hdf5(total_hdf5_path, H5T_NATIVE_FLOAT, subdir_cadence,
      milliseconds_per_file, global_start_sample, sample_rate_numerator, SAMPLE_RATE_DENOMINATOR,
     "TangerineSDR", 0, 0, 1, noOfChannels, 1, 1);
      }

// here we write out DRF

    vector_sum = vector_leading_edge_index + hdf_i*sampleCount; 

// push buffer directly to DRF just like it is
    if(DRFdata_object != NULL)  // make sure there is an open DRF file
	  {

      result = digital_rf_write_hdf5(DRFdata_object, vector_sum, buf_ptr->theDataSample,sampleCount) ;
	//  fprintf(stderr,"DRF write result = %d, vector_sum = %ld \n",result, vector_sum);
	  }

    hdf_i++;  // increment count of hdf buffers processed

    free (buf_ptr);  // free the work buffer
    }
  }

  free(buf->base);  // free the callback buffer
  return;  // end of callback for handling incoming I/Q data
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

// the following function prepares data in ring buffer for upload 
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
  printf("input to extract from= '%s'\n", input);
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

void recv_port_check() {
  if(recv_port_status == 0)   // if port not already open, open it
      {
// start a listener on Port F
      uv_udp_init(loop, &data_socket);
      uv_ip4_addr("0.0.0.0", LH_DATA_IN_port, &recv_data_addr);
      printf("I/Q DATA: start listening on port %u\n",htons(recv_data_addr.sin_port));
      int retcode = uv_udp_bind(&data_socket, (const struct sockaddr *)&recv_data_addr, UV_UDP_REUSEADDR);
      printf("bind retcode = %d\n",retcode);
      retcode = uv_udp_recv_start(&data_socket, alloc_data_buffer, on_UDP_data_read);
      printf("recv retcode = %d\n",retcode);
      if (retcode == 0) recv_port_status = 1;
      }
  else
    puts("recv port already open");
}

/////////////////////////////////////////////////////////////////////
// callback for when a command is received from webcontroller
/////////////////////////////////////////////////////////////////

void process_command(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
  puts("process_command routine triggered\n");
  if(nread == -4095)  // TODO: this seems to be junk coming from flask app (?) - need to fix
	{ puts("ignore 1 buffer"); return;
	}
  if (nread < 0) {
    fprintf(stderr, "Webcontroller Read error, nread = %ld\n",nread);  // DE disconnected/crashed
    {  // inform webcontrol that DE seems unresponsive
     uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
     puts("set up write_req");
     uv_buf_t a[]={{.base="NAK", .len=3},{.base="\n",.len=1}};
     puts("Send NAK status to webcontrol");
  //   uv_write_t WBdata;

     uv_write(write_req, (uv_stream_t*) webStream, a, 2, web_write_complete);
    }
    puts("do uv_close");
    uv_close((uv_handle_t*)client, NULL);
    puts("free read buffer");
    free(buf->base);  // release memory allocated to read buffer
    return ;
    }
  printf("Command received from web control: %c%c len= %zd\n", buf->base[0], buf->base[1], nread);

  char mybuf[200];

  memset(&mybuf, 0, sizeof(mybuf));
  strncpy(mybuf, buf->base, nread-1);   // subtract 1 to strip CR
//  strncpy(d.mybuf1,buf->base, nread-1);  // get a copy se we can dissect it
// NOTE! if controller does not send \n at end of buffer, commmand will be truncated (above)
  

  if(strncmp(mybuf, CREATE_CHANNEL, 2)==0)  // Request to create a configureation/data channel pair
	{
 //   printf("Create Channel received at maintcl; port1=%s \n",d.c.port1);
    uv_udp_send_t send_req;
	char b[200];
//	for(int i=0; i< 100; i++) { b[i] = 0; }
    CONFIGBUF *configBuf_ptr;
    configBuf_ptr = (CONFIGBUF *)malloc(sizeof(CONFIGBUF));  // TODO: free this later
    memcpy(configBuf_ptr->cmd, mybuf,2);

    const char comma[2] = ",";
    char *token;
    token = strtok(mybuf, comma);
    printf("initial token = %s\n", token);
    token = strtok(NULL, comma);

    printf("second token = %s\n", token);
//  Set up "Port C" where we listen for config request response
    int ret = sscanf(token,"%5hu",&LH_CONF_IN_port );
    printf("port conversion done, ret= %d\n",ret);
    printf("Port C assigned as %d \n",LH_CONF_IN_port);
//    memcpy(configBuf_ptr->configPort,LH_CONF_IN_port,2);
//  Set up "Port F" where we will listen for I/Q data coming from DE
    configBuf_ptr->configPort = LH_CONF_IN_port;
    token = strtok(NULL, comma);
    
    printf("third token = %s\n", token);

    ret = sscanf(token,"%5hu",&LH_DATA_IN_port );
    printf("port conversion done, ret= %d\n",ret);
    printf("Port F assigned as %d \n",LH_DATA_IN_port);

    configBuf_ptr->dataPort = LH_DATA_IN_port;
    memcpy(b,configBuf_ptr,sizeof(CONFIGBUF));
    puts("port C print done");

	const uv_buf_t a[] = {{.base = b, .len = sizeof(CONFIGBUF)}};

    printf("Sending CREATE CHANNEL to %s  port %u\n", DE_IP, DE_port);
    uv_ip4_addr(DE_IP, DE_port, &send_addr);    
    uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    printf("... at this point, send_config_addr.sin_port= %d\n", ntohs(send_config_addr.sin_port));
    free(configBuf_ptr);
    return;
	}

////////////////////////////////////////////////////////////////////
  if(strncmp(mybuf, CONFIG_CHANNELS, 2)==0) 
    {
    uv_udp_send_t send_req;
    char b[400];
    printf("Config channels (CH) received=%s\n",mybuf);
	memcpy(h.channelBuffer.chCommand, CONFIG_CHANNELS, sizeof(CONFIG_CHANNELS));  // Put the command into buf
   
    const char comma[2] = ",";
    char *token;
    token = strtok(mybuf, comma);
    printf("initial token = %s\n", token);
    token = strtok(NULL, comma);   // second token is # active channels
    printf("second token (# channels) = %s\n", token);
    h.channelBuffer.activeChannels = atoi(token);
    token = strtok(NULL, comma);   
    printf("third token (data rate) = %s\n", token);
    h.channelBuffer.channelDatarate = atoi(token);
    sample_rate_numerator= atoi(token); // set this for data acquisition

    for (int i=0; i < h.channelBuffer.activeChannels; i++)
      {
      token = strtok(NULL, comma);
      printf("Channel# %s :\n",token);
 //     printf("next token = %s\n", token);
      int ret = sscanf(token,"%i",&h.channelBuffer.channelDef[i].channelNo );
      printf("converted to %i \n",h.channelBuffer.channelDef[i].channelNo);
      token = strtok(NULL, comma);
      printf("port = %s\n", token);
      ret = sscanf(token,"%i",&h.channelBuffer.channelDef[i].antennaPort);
      printf("port converted to %i \n",h.channelBuffer.channelDef[i].antennaPort);
      token = strtok(NULL, comma);
      printf("next token = %s\n", token);
      ret = sscanf(token,"%lf",&h.channelBuffer.channelDef[i].channelFreq);
      printf("freq converted to %lf \n",h.channelBuffer.channelDef[i].channelFreq);
 
      }
    puts("done with conversion");

    memcpy(b,h.mybuf2,sizeof(CHANNELBUF));
    puts("port C print done");

	const uv_buf_t a[] = {{.base = b, .len = sizeof(CHANNELBUF)}};

    printf("Sending CONFIG CHANNELS to %s  port %u\n", DE_IP, d.myConfigBuf.dataPort);
    uv_ip4_addr(DE_IP, d.myConfigBuf.dataPort, &send_config_addr);  
      
 //   uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    printf("port in the addr = %i\n",ntohs(send_config_addr.sin_port));
    uv_udp_send(&send_req, &send_config_socket, a, 1, (const struct sockaddr *)&send_config_addr, on_UDP_send);
    printf("after sending, port in the addr = %i\n",ntohs(send_config_addr.sin_port));
    }

  if(strncmp(mybuf, START_FT8_COLL , 2)==0)
    {
    uv_udp_send_t send_req;
	char b[100];
	for(int i=0; i< 100; i++) { b[i] = 0; }
    recv_port_check();  // ensure receive port is open (LH_DATA_IN_port)
	strncpy(b, mybuf, nread-1 );
	const uv_buf_t a[] = {{.base = b, .len = nread-1}};
    int rt =     system("mkdir /mnt/RAM_disk/FT8");
    rt = system("rm /mnt/RAM_disk/FT8/*.*");
    printf("Sending START FT8  to %s  port %u\n", DE_IP, DE_port);
    uv_ip4_addr(DE_IP, DE_port, &send_addr);    
    uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    return;
	}

  if(strncmp(mybuf, STOP_FT8_COLL , 2)==0)
    {
    uv_udp_send_t send_req;
	char b[60];
	for(int i=0; i< 60; i++) { b[i] = 0; }
	strcpy(b, STOP_FT8_COLL );
	const uv_buf_t a[] = {{.base = b, .len = 2}};
    printf("Sending STOP FT8  to %s  port %u\n", DE_IP, DE_port);
    uv_ip4_addr(DE_IP, DE_port, &send_addr);    
    uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    return;
	}

  if(strncmp(mybuf, START_DATA_COLL , 2)==0)
	{
    printf("M: START DATA COLL COMMAND RECEIVED\n");
// determine what mode to run
   num_items = rconfig("mode",configresult,0);
   if(strncmp(configresult,"snapshotter",11)==0)
    {
    printf("STARTING SNAPHOTTER mode\n");
    snapshotterMode = 1;
    ringbufferMode = 0;
    snapcount = 0;
    }
   if(strncmp(configresult, "ringbuffer",10) == 0)
    {
    printf("STARTING RINGBUFFER mode\n");
    ringbufferMode = 1;
    snapshotterMode = 0;
    snapcount = 0;
    }
   if(strncmp(configresult,"r+s",3)==0)
    {
    printf("STARTING SNAPSHOTTER AND RINGBUFFER modes\n");
    ringbufferMode = 1;
    snapshotterMode = 1;
    snapcount = 0;
    }

   if(snapshotterMode)
    {

 // User requested to start data collection
 // See how many channels aqre set to run
      num_items = rconfig("numchannels",configresult,0);
      if(num_items == 0)
       {
       printf("ERROR - numchannels setting not found in config.ini");
       }
      else
       {
        printf(" CONFIG RESULT = '%s'\n",configresult);
        printf("len =%lu\n",strlen(configresult));
        numchannels = atoi(configresult);
       }
     if(!FFTmemset)
     {
      for (int i = 0; i < numchannels; i++)  // allocate memory for FFT(s)
        {
        printf("FFTW create plans %i\n",i);
        spectrumPackage[i].channelNo = i;
    //    spectrumPackage[i].spectrum_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_N);
    //    spectrumPackage[i].FFTout = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_N);
        spectrumPackage[i].p = fftwf_plan_dft_1d(FFT_N, spectrumPackage[i].spectrum_in, spectrumPackage[i].FFTout, FFTW_FORWARD, FFTW_ESTIMATE);
        printf("plans created for fft %i\n",i);
        }
       FFTmemset = 1; // indicate that memory is allocated
      }
    }


    uv_udp_send_t send_req;
 //   printf("M: try to print subdir, 1\n");
	char b[60];
	for(int i=0; i< 60; i++) { b[i] = 0; }
 //   printf("M: ry to print subdir 2\n");
	strcpy(b, START_DATA_COLL );
  //  const char* frombuf = mybuf;
    for(int z=0; z < 15; z++)
     {
     hdf5subdirectory[z] = mybuf[z+3];
     }
    hdf5subdirectory[16]=0;
 //   printf("M: try to print subdir 3\n");
    printf("HDF5 subdirectory = %s\n",hdf5subdirectory);
	const uv_buf_t a[] = {{.base = b, .len = 2}};
    if(recv_port_status == 0)   // if port not already open, open it
      {
// start a listener on Port F
      recv_port_check(); // ensure port is open exactly once
/*
      uv_udp_init(loop, &data_socket);
      uv_ip4_addr("0.0.0.0", LH_DATA_IN_port, &recv_data_addr);
      printf("I/Q DATA: start listening on port %u\n",htons(recv_data_addr.sin_port));
      int retcode = uv_udp_bind(&data_socket, (const struct sockaddr *)&recv_data_addr, UV_UDP_REUSEADDR);
      printf("bind retcode = %d\n",retcode);
      retcode = uv_udp_recv_start(&data_socket, alloc_data_buffer, on_UDP_data_read);
      printf("recv retcode = %d\n",retcode);
      if (retcode == 0) recv_port_status = 1;
*/
      }

    printf("Sending START DATA COLLECTION  to %s  port %u\n", DE_IP, DE_port);
    uv_ip4_addr(DE_IP, DE_port, &send_addr);    
    uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    return;
	}

/////////  Handle command to stop data collection ////////////////
  if(strncmp(mybuf, STOP_DATA_COLL , 2)==0)
	{
    uv_udp_send_t send_req;
	char b[60];
	for(int i=0; i< 60; i++) { b[i] = 0; }
	strcpy(b, STOP_DATA_COLL);
	const uv_buf_t a[] = {{.base = b, .len = 2}};
    struct sockaddr_in send_addr;
    printf("Sending STOP DATA COLLECTION  to %s  port %u\n", DE_IP, DE_port);
    uv_ip4_addr(DE_IP, DE_port, &send_addr);    
    uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    sleep(0.25);  // wait for command to be processed
    // now close the DRF file
    int result = digital_rf_close_write_hdf5(DRFdata_object);  
    DRFdata_object = NULL;
    fprintf(stderr,"DRF close, result = %d \n",result);

/*
     for (int i = 0; i < numchannels; i++)  // allocate memory for FFT(s)
      {
      printf("FFTW deallocate memory %i\n",i);
      fftwf_destroy_plan(spectrumPackage[i].p);
      fftwf_free(spectrumPackage[i].spectrum_in);
      fftwf_free(spectrumPackage[i].FFTout);

      printf("deallocated mem for fft %i\n",i);
      }
  //   fftwf_cleanup();
*/


    return;
	}


  if(strncmp(mybuf, STATUS_INQUIRY, 2)==0)
	{
	puts("Forward status inquiry to DE");
    uv_udp_send_t send_req;
	char b[60];
	for(int i=0; i< 60; i++) { b[i] = 0; }
  // status inquiry
	strcpy(b, "S?");
	const uv_buf_t a[] = {{.base = b, .len = 2}};
    struct sockaddr_in send_addr;
    printf("Sending STATUS INQUIRY to %s  port %u\n", DE_IP, DE_port);
    printf("\t(Listening on port %d )\n", ntohs(recv_addr.sin_port));
    uv_ip4_addr(DE_IP, DE_port, &send_addr);    
    uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    return;
	}

  if(strncmp(mybuf, HALT_DE, 2)==0)  // acts to restart the DE
	{
	puts("Forward status inquiry to DE");
    uv_udp_send_t send_req;
	char b[60];
	for(int i=0; i< 60; i++) { b[i] = 0; }
  // status inquiry
	strcpy(b, "XX");
	const uv_buf_t a[] = {{.base = b, .len = 2}};
    struct sockaddr_in send_addr;
    printf("Sending RESTART to %s  port %u\n", DE_IP, DE_port);
  //  printf("\t(Listening on port %d )\n", ntohs(recv_addr.sin_port));
    uv_ip4_addr(DE_IP, DE_port, &send_addr);    
    uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    return;
	}

// this should be in the UDP data handler
  if(strncmp(mybuf, DATARATE_INQUIRY, 2)==0)
	{
	puts("Forward datarate inquiry to DE");
    uv_udp_send_t send_req;
	char b[60];
	for(int i=0; i< 60; i++) { b[i] = 0; }
  // status inquiry
	strcpy(b, "R?");
	const uv_buf_t a[] = {{.base = b, .len = 2}};
    struct sockaddr_in send_addr;
    printf("Sending DATARATE INQUIRY to %s  port %u\n", DE_IP, DE_port);
  //  printf("\t(Listening on port %d )\n", ntohs(recv_addr.sin_port));
    uv_ip4_addr(DE_IP, DE_port, &send_addr);    
    uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    return;
	}


  if(strncmp(buf->base,"BYE",3)==0)
    {
    puts("halting");
    uv_stop(loop);
    }


// TODO: following code probably never reached
  if(strncmp(buf->base, START_DATA_COLL,2)==0)  // is this a command to start collecting data?
    {
      buffers_received = 0;  // this lets us count buffers independently of counter in the buffer itself
    }


// try to close DRF file, but only if one is open
  puts("check if DRF file open; if so, close it");
  if(strncmp(buf->base, STOP_DATA_COLL,2)==0 && DRFdata_object != NULL)
	{
      puts("Closing DRF file");
	  result = digital_rf_close_write_hdf5(DRFdata_object);
      DRFdata_object = NULL;
	}

  }


/////////////////////////////////////////////////////////////////
// callback for when a connection from webcontroller is received
////////////////////////////////////////////////////////////////
void on_new_connection(uv_stream_t *server, int status) {
  if (status == -1) {
    return;
  }
  puts("new incoming connection.");
  uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, client);
  if (uv_accept(server, (uv_stream_t*) client) == 0) {
    uv_read_start((uv_stream_t*) client, alloc_buffer, process_command);
    webStream = (uv_stream_t*) client;  // save stream object for replies to webserver
  }
  else {
    uv_close((uv_handle_t*) client, NULL);
  }
}

/////////////////////////////////////////////////////////////
// callback after write to DE is completed
////////////////////////////////////////////////////////////
void on_DE_write(uv_write_t* req, int status)
{
  if (status) {
   // uv_errno_t err = uv_strerror(loop);
   // fprintf(stderr, "uv_write error: %s\n", uv_strerror(err));
    fprintf(stderr, "uv_write error: %d\n", status);
		return;
    }
	printf("wrote.\n");
	//free(req);
}

/////////////////////////////////////////////////////////////////
// callback indicating client-type connection to DE complete
///////////////////////////////////////////////////////////////
void on_DE_CL_connect(uv_connect_t* connection, int status)
{
    printf("Status = %d\n", status);
	if (status == -ECONNREFUSED)
		{
		printf("Connection refused; DE not running or not connected\n");
		}
	else
		printf("connected to DE.\n");
	uv_stream_t* stream = connection->handle;
    DEsockethandle = connection ->handle;
	uv_buf_t buffer[] = {
		{.base = "S?", .len = 2},
		{.base = "S?", .len = 2}
	 };
	uv_write_t request;
   // removed temporarily
	// uv_write(&request, stream, buffer, 1, on_DE_write);
    puts("Make ready to receive data from DE");
    uv_read_start((uv_stream_t*) stream, alloc_buffer, handleDEdata);
}


/////////////////////////////////////////////////

void statusInquiry() {
    puts("called status inquiry");
    uv_udp_send_t send_req;
	char b[60];
	for(int i=0; i< 60; i++) { b[i] = 0; }
	strcpy(b, "S?");
	const uv_buf_t a[] = {{.base = b, .len = 2}};
    struct sockaddr_in send_addr;
    printf("Sending STATUS INQUIRY to %s  port %u\n", DE_IP, DE_port);
    uv_ip4_addr(DE_IP, DE_port, &send_addr);    
    uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
    return;
}


///////////////////////////////////////////////////////////////
//  Callback for when UDP command/ACK/NAK packets received from DE 
//  A separate callback handles incoming I/Q data, above
//////////////////////////////////////////////////////////////
void on_UDP_read(uv_udp_t * recv_handle, ssize_t nread, const uv_buf_t * buf,
		const struct sockaddr * addr, unsigned flags)
  {
  int channelPtr;
  if(nread == 0 )
    { 
   //   puts("received UDP zero");
      free(buf->base);
	  return;
    }
  printf("UDP data recvd, bytes = %ld\n", nread);

  DATABUF *buf_ptr;
  buf_ptr = (DATABUF *)malloc(sizeof(DATABUF));  // allocate memory for working buf
// TODO: ensure there is a free for this memory before every return
  memcpy(buf_ptr, buf->base,nread);    // get data from UDP buffer

 // printf("DE BUFTYPE = %s \n",buf_ptr->bufType);

  // respond to STATUS INQUIRY

  if(strncmp(buf_ptr->bufType, "OK" ,2) ==0)
	{
	puts("OK status message received from DE!  It's alive!!");
    uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    puts("set up write_req");
    uv_buf_t a[]={{.base="OK", .len=2},{.base="\n",.len=1}};
    puts("Send OK status to webcontrol");
    uv_write(write_req, (uv_stream_t*) webStream, a, 2, web_write_complete);
    free(buf_ptr);
    return;
	}

  if(strncmp(buf_ptr->bufType, DATARATE_RESPONSE, 2)==0)
	{
     DATARATEBUF *rbuf_ptr;
     rbuf_ptr = (DATARATEBUF *)malloc(sizeof(DATARATEBUF));  // allocate memory for working buf

    COMBOBUF cbuf;
    memcpy(&cbuf.dbuf, buf->base,nread);    // get data from UDP buffer
//  strncpy(cbuf.dbufc, buf->base,nread,sizeof(DATARATEBUF));
  printf("buffer = %s \n",cbuf.dbufc);

  char b[200] = "DR:";
  char c[20];
  for(int i = 0; i < 16; i++)
   {
    if(cbuf.dbuf.dataRate[i].rateNumber == 0 ) break;
    sprintf(c,"%i,%i;",cbuf.dbuf.dataRate[i].rateNumber,cbuf.dbuf.dataRate[i].rateValue);
    strcat(b,c);
   };

  printf("DR string= %s\n",b);

 // printf("DR buf type found %s \n",rbuf_ptr->buftype);
  printf("intial entry %i %i \n",cbuf.dbuf.dataRate[0].rateNumber,                  cbuf.dbuf.dataRate[0].rateValue); 

	puts("Forward datarate response to webcontrol");
    uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    puts("set up write_req");
    uv_buf_t a[]={{.base=b, .len=sizeof(COMBOBUF)},{.base="\n",.len=1}};
    for(int i=0; i < 16; i++)
     {
     printf("%X ",cbuf.dbufc[i]);
     };
      printf("\n");

    uv_write(write_req, (uv_stream_t*) webStream, a, 2, web_write_complete);
 //   puts("Send DR list to webcontrol");
  //  uv_write(write_req, (uv_stream_t*) webStream, buf, sizeof(DATARATEBUF), web_write_complete);
    free(rbuf_ptr);
    return;
	}

  if(strncmp(buf_ptr->bufType, "AK" ,2) ==0)
    {
    printf("AK buffer contains %x %x %x %x %x %x \n",
           buf->base[0], buf->base[1], buf->base[2],
			buf->base[3], buf->base[4], buf->base[4]);

    if(nread == 6)    // this should be a CC ACK containing DE listening ports
      {
      puts("Prep to handle incoming IQ data");
      memcpy(d.mybuf1, buf->base, 6);
      printf("AK received from last command:  %s ,", d.myConfigBuf.cmd);
      printf("DE receive ports = %hu  %hu \n", d.myConfigBuf.configPort, d.myConfigBuf.dataPort);
      DE_DATA_IN_port = d.myConfigBuf.dataPort;
      uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));

      uv_buf_t a[]={{.base="ACK", .len=3},{.base="\n",.len=1}};
      puts("Forward the ACK");
//    Forward ACK to web controller 
      uv_write(write_req, (uv_stream_t*) webStream, a, 2, web_write_complete);
      return;
      }

    uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    uv_buf_t a[]={{.base="ACK", .len=3},{.base="\n",.len=1}};
    puts("Forward the ACK");
    uv_write(write_req, (uv_stream_t*) webStream, a, 2, web_write_complete);
    return;
    }

//  If DE could not fulfil the last request or command, it sends a NAK
    if(strncmp(buf_ptr->bufType, "NAK" ,3) ==0)
    {
    puts("NAK received from last command");
    return;
    }

////  start of handling incoming I/Q data //////////////

    if(strncmp(buf_ptr->bufType, "FT" ,2) ==0)  // this is a buffer of FT8
    {
       double dialfreq = buf_ptr->centerFreq;
       channelPtr = buf_ptr-> channelNo;
       printf("FT8 data, f = %f, buf# = %ld \n",buf_ptr->centerFreq, buf_ptr->dval.bufCount);
       if(buf_ptr->dval.bufCount == 0 )   // this is the first buffer of the minute
       {
        t = time(NULL);

        if((gmt = gmtime(&t)) == NULL)
          { fprintf(stderr,"Could not convert time\n"); }
        strftime(date, 12, "%y%m%d_%H%M", gmt);
        sprintf(name[channelPtr], "/mnt/RAM_disk/FT8/ft8_%d_%f_%d_%s.c2", 1, buf_ptr->centerFreq,1,date);
       if((fp[channelPtr] = fopen(name[channelPtr], "wb")) == NULL)
        { fprintf(stderr,"Could not open file %s \n",name[channelPtr]);
          return;
        }
       fwrite(&dialfreq, 1, 8, fp[channelPtr]);
      }
      fwrite(buf_ptr->theDataSample, 1, 8000, fp[channelPtr]);
      if (buf_ptr->dval.bufCount == 239 )   // was this the last buffer?
        {
        fclose(fp[channelPtr]);
        char chstr[2];
        sprintf(chstr,"%d",channelPtr);
        char mycmd[100];
        strcpy(mycmd, "./ft8d ");
        strcat(mycmd,name[channelPtr]);
        strcat(mycmd, "  > /mnt/RAM_disk/FT8/decoded");
        strcat(mycmd, chstr);
        strcat(mycmd, ".txt &");
        printf("the command: %s\n",mycmd);
        int ret = system(mycmd);
        puts("ft8 decode ran");
        }
      return;
    }

  buffers_received++;
  //fprintf(stderr,"received UDP packet# %zd\n",packetCount);
  fprintf(stderr,"nread = %zd\n", nread);
  fprintf(stderr,"Buffer starts with: %02x %02X \n",buf->base[0], buf->base[1]);
  if (nread < 8000)  // discard this buffer for now 
	{
	fprintf(stderr, "Buffer is < 8000 bytes;  * * * IGNORE * * *\n");
	}
  if (nread > 8000)  // looks like a valid databuffer, based on length
	{
	printf("\n\n*** ERROR *** - Data buffer incorrectly processed\n\n");
	}

  free(buf->base);

  }

///////////////////// open config file ///////////////
int openConfigFile()
{
  printf("test - config init\n");
  config_init(&cfg);

  /* Read the file. If there is an error, report it and exit. */

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


////////////// Heartbeat to Central Host ////////////////////

void *heartbeat(void *threadid) {

int portno = 5000;  // TODO: use config value here

char *host = "192.168.1.67";  // TODO: use config value here

char *message_fmt = "POST /apikey/SHGJKD HTTP/1.0\r\n\r\n";

struct hostent *server;
struct sockaddr_in serv_addr;
struct timeval timeout;
int sockfd, bytes, sent, received, total;
char message[1024],response[4096];

timeout.tv_sec = 10;  // if Central Host doesn't respond, we ignore
timeout.tv_usec = 0;  //  and will try again after the pause time

//if (argc < 3) { puts("Parameters: <apikey> <command>"); exit(0); }

/* fill in the parameters */
//sprintf(message,message_fmt,argv[1],argv[2]);
//sprintf(message,message_fmt);

char tbuf[30];
struct timeval tv;
time_t curtime;
int heartbeat_interval = 60;  // seconds

while(1==1)  // heartbeat loop
{
  char mytoken[75];

  num_items = rconfig("token_value",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - token_value setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT = '%s'\n",configresult);
    printf("len =%lu\n",strlen(configresult));
    strcpy(mytoken, configresult);
    }

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

  gettimeofday(&tv, NULL);
  curtime = tv.tv_sec;
  strftime(tbuf, 30, "%m-%d-%Y-%T.",localtime(&curtime));
  printf("Heartbeat TOD: '%s' %ld\n",tbuf,tv.tv_usec);
  sprintf(message,"POST /apikey/%s-%s HTTP/1.0\r\n\r\n",mytoken,tbuf);
//strcpy(message,"HB!");
  printf("M: hearbeat - set up socket\n");
/* create the socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) printf("M: Heartbeat thread- ERROR opening socket\n");
/* set up to allow timeouts, in case we can't hit Central Control */
  if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
        sizeof(timeout)) < 0)
       printf("M: Set socket option rcv timeout failed\n");
  if (setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
        sizeof(timeout)) < 0)
       printf("M: Set socket option send timeout failed\n");

/* lookup the ip address */
  server = gethostbyname(host);
  if (server == NULL) printf("M: Heartbeat thread - ERROR, no such host\n");

/* fill in the structure */
  memset(&serv_addr,0,sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(portno);
  memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

/* connect the socket */
  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
    {
    printf("M: Heartbeat - ERROR connecting\n");
    sleep(heartbeat_interval);
    continue;
    }
  else
    printf("Heartbeat socket connected\n");

/* send the request */
  printf("M: hearbeat - send request: %s\n",message);
  total = strlen(message);
  sent = 0;
  do {
   bytes = write(sockfd,message+sent,total-sent);
   if (bytes < 0)
     printf("M: Heartbeat - ERROR writing message to socket\n");
   else
      printf("Heartbeat sent byte, return = %i\n", bytes);
   if (bytes == 0)
      break;
   sent+=bytes;
  } while (sent < total);

/* receive the response */
  memset(response,0,sizeof(response));
  total = sizeof(response)-1;
  received = 0;
  do {
   bytes = read(sockfd,response+received,total-received);
   if (bytes < 0)
    printf("M: Heartbeat - ERROR reading response from socket\n");
   else
    printf("M: Heartbeat got from Central %i bytes\n",bytes);
   if (bytes == 0)
     break;
  received+=bytes;
  } while (received < total);

  if (received == total)
    printf("M: Heartbeat - ERROR storing complete response from socket");

/* process response */
  printf("M: Heartbeat Response:\n%s\n",response);

  char theStart[30];
  char theEnd[30];
  if(getDataDates(response, &theStart[0], &theEnd[0]))
   {
   printf("M: Received a DR data request from Central\n");

   if(!uploadInProgress)
    {
    int rp = prep_data_files(theStart, theEnd, ringbuffer_path);
    int uplrc = 0;
    long h;
    pthread_t uplthread;
    printf("M: Start upload thread\n");
    uplrc = pthread_create(&uplthread, NULL, dataUpload, (void*)h);
    }
   }

  sleep(heartbeat_interval);

/* close the socket */
  close(sockfd);
}

return 0;

}  // end of heartbeat thread



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
  strcpy(pathToRAMdisk, "/mnt/RAM_disk");

  int r = openConfigFile();
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


//////////////////////////////////////////////////////
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
          (NULL == CU_add_test(pSuite, "testUploadThread", testUploadThread))
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
  puts("starting");
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
		DE_port = htons(discovered[i].info.network.address.sin_port);
		printf("selected Tangerine at port %u (Port B)\n",DE_port);
	  }
	}

/*
// the configuration file
  config_t cfg;
  config_setting_t *setting;
*/

  char DE_ip_str[20];
  packetCount = 0;
  int DE_port;
  int controller_port;

  int rc = openConfigFile();

/*
  config_init(&cfg);

  /* Read the file. If there is an error, report it and exit. */

// The only thing we use this config file for is to get the path to the
// python config file. Seems like a kludge, but allows flexibility in
// system directory structure.
/*
  if(! config_read_file(&cfg, "/home/odroid/projects/TangerineSDR-notes/mainctl/main.cfg"))
  {
    fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
            config_error_line(&cfg), config_error_text(&cfg));
    puts("ERROR - there is a problem with main.cfg configuration file");
    config_destroy(&cfg);
    return(EXIT_FAILURE);
  }

  if(config_lookup_string(&cfg, "config_path", &configPath))
    printf("Setting config file path to: %s\n\n", configPath);
  else
    fprintf(stderr, "No 'config_path' setting in configuration file main.cfg.\n");
*/

  puts("start");
 // printf("looking for '%s'\n",target);
  num_items = rconfig("configport",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - configport setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT = '%s'\n",configresult);
 //   printf("len =%lu\n",strlen(result));
    LH_CONF_IN_port = atoi(configresult);
    num_items = rconfig("dataport",configresult,0);
    LH_DATA_IN_port = atoi(configresult);
    }

//  strcpy(target,"DE_ip");
//  int num_items = 0;
  puts("start");
 // printf("looking for '%s'\n",target);
  num_items = rconfig("DE_ip",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - DE_ip setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT for DE_ip = '%s'\n",configresult);
    printf("len =%lu\n",strlen(configresult));
    strcpy(DE_ip_str, configresult);
    }

  num_items = rconfig("DE_port",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - DE_ip setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT for DE_port = '%s'\n",configresult);
    printf("len =%lu\n",strlen(configresult));
    DE_port = atoi(configresult);
    }

  num_items = rconfig("controlport",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - controlport setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT = '%s'\n",configresult);
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
    printf(" CONFIG RESULT = '%s'\n",configresult);
    printf("len =%lu\n",strlen(configresult));
   

  num_items = rconfig("subdir_cadence",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - subdir_cadence setting not found in config.ini");
    }
  else
    {
    printf(" subdir cadence CONFIG RESULT = '%s'\n",configresult);
    printf("len =%lu\n",strlen(configresult));
    subdir_cadence = atoi(configresult);
    }

  num_items = rconfig("milliseconds_per_file",configresult,0);
  if(num_items == 0)
    {
    printf("ERROR - milliseconds_per_file setting not found in config.ini\n");
    }
  else
    {
    printf(" subdir cadence CONFIG RESULT = '%s'\n",configresult);
    printf("len =%lu\n",strlen(configresult));
    milliseconds_per_file  = atoi(configresult);
    }

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
    printf("central port CONFIG RESULT = '%s'\n",configresult);
    strcpy(pathToRAMdisk,configresult);
    } 

  loop = uv_default_loop();

// Set up to listen to incoming TCP port for commands from web controller
  uv_tcp_t server;
  uv_tcp_init(loop, &server);
  struct sockaddr_in bind_addr;
  uv_ip4_addr("0.0.0.0", 6100, &bind_addr);
  puts("Bind to input (terminal) port for listening");
  uv_tcp_bind(&server, (struct sockaddr *)&bind_addr, 0);
  int r = uv_listen((uv_stream_t*) &server, 128, on_new_connection);
  if (r) {
    fprintf(stderr, "TCP Listen error!\n");
  //  return 1;
    }
  
  puts("prep to receive UDP data");
  uv_udp_t recv_socket;
  uv_udp_init(loop, &recv_socket);

 // here we will listen on that port broadcast earlier selected ("Port A")
  uv_ip4_addr("localhost", LH_port, &recv_addr);   // TODO: should be localhost (?)
  printf("will listen on port A %u \n", LH_port);
  uv_udp_bind(&recv_socket, (const struct sockaddr *)&recv_addr, UV_UDP_REUSEADDR);
  uv_udp_recv_start(&recv_socket, alloc_buffer, on_UDP_read);

  // here we bind (for sending) to socket using same port as reeciving on
  puts("prep to send UDP data (commands)");

  uv_udp_init(loop, &send_socket);
  uv_ip4_addr(INADDR_ANY, LH_port, &send_addr);
  uv_udp_bind(&send_socket, (const struct sockaddr *)&send_addr, 0);

/////////////////////////////////////////////////////////////////
// here we will set up to send and receive on "Port C"

  puts("Prep to handle config request and reply");
  uv_udp_t config_socket;
  uv_udp_init(loop, &config_socket);
// Set up "Port C" where we listen for response from DE to config request
  uv_ip4_addr("0.0.0.0", LH_CONF_IN_port , &recv_config_addr); 
  printf("will listen on port C %u \n", LH_CONF_IN_port);
  uv_udp_bind(&config_socket, (const struct sockaddr *)&recv_config_addr, UV_UDP_REUSEADDR);
  uv_udp_recv_start(&config_socket, alloc_config_buffer, on_UDP_read);

  printf("prep to send UDP config (CH) using outbound port %i\n", LH_CONF_IN_port);
// TODO: this must be the configured port
  uv_udp_init(loop, &send_config_socket);
  uv_ip4_addr("255.255.255.255", 50001, &send_config_addr);
//  send_config_addr.sin_addr.s_addr = htonl(INADDR_ANY);
//send_config_addr.sin_family = AF_INET;
 // send_config_addr.sin_port = htons(LH_CONF_IN_port);  // can we force it?
  uv_udp_bind(&send_config_socket, (const struct sockaddr *)&send_config_addr, 0);
  printf("UDP config port setup done\n");


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
    pthread_t hbthread;
    printf("M: Start hearbeat thread\n");
    hbrc = pthread_create(&hbthread, NULL, heartbeat, (void*)h);

// Set up memory & plan for FFT
  //  FFTin =  (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FFT_N);
  //  FFTout =  (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FFT_N);
   // FFTp = fftwf_plan_dft_1d(FFT_N, FFTin, FFTout, FFTW_FORWARD, FFTW_ESTIMATE);



////////////////////////////////////////////////////////////////


  return uv_run(loop, UV_RUN_DEFAULT);
}
