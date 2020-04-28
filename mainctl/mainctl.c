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
static	uint64_t sample_rate_numerator = 48000; // simulating here
static	uint64_t sample_rate_denominator = 1;
static	uint64_t subdir_cadence = 400; /* Number of seconds per subdirectory - typically longer */
static	uint64_t millseconds_per_file = 4000; /* Each subdirectory will have up to 10 400 ms files */
static	int compression_level = 0; /* low level of compression */
static	int checksum = 0; /* no checksum */
static	int is_complex = 1; /* complex values */
static	int is_continuous = 1; /* continuous data written */
static	int num_subchannels = 1; /* subchannels */
static	int marching_periods = 1; /*  marching periods when writing */
static	char uuid[100] = "DE output";
static  char hdf5File[50];   // = "/media/odroid/416BFA3A615ACF0E/hamsci/hdf5";  // TODO: set based on python config
//static  char hdf5File1[50] = "/tmp/RAM_disk/ch4_7";
static uint64_t theUnixTime = 0;
//static  char hdf5File[50] = "/tmp/ramdisk";
//char *filename = "/media/odroid/hamsci/raw_data/dat3.dat";  // for testing raw binary output
static  char* sysCommand1;
static  char* sysCommand2;
static	uint64_t vector_length = 1024; /* number of samples written for each call -  */
static  uint64_t vector_sum = 0;

static long buffers_received = 0;  // for counting UDP buffers rec'd in case any dropped in transport

static char ringbuffer_path[50];
//const  char *ringbufferPath;

static long packetCount;
static int recv_port_status = 0;
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

struct sockaddr_in send_addr;  // used for sending commands to DE
struct sockaddr_in recv_addr;  // for command replies from DE (ACK, NAK)

struct sockaddr_in send_config_addr;  //used for sending config req (CH) to DE
struct sockaddr_in recv_config_addr;  // for config replies from DE

struct sockaddr_in recv_data_addr;   // for data coming from DE

static long packetCount;

// variables for FT8 reception
char date[12];
char name[8][64];
time_t t;
struct tm *gmt;
FILE *fp[8];

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


//// *********************** Start of Code  *******************************//////

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
      puts("received UDP zero");
      free(buf->base);
	  return;
    }
  printf("UDP I/Q data recvd, bytes = %ld\n", nread);

  DATABUF *buf_ptr;
  buf_ptr = (DATABUF *)malloc(sizeof(DATABUF));  // allocate memory for working buf
  memcpy(buf_ptr, buf->base,nread);    // get data from UDP buffer
  printf("DE BUFTYPE = %s \n",buf_ptr->bufType);

/*
  if(buf_ptr->bufType[0] == 0x44  && buf_ptr->bufType[1] == 0x52)
    printf("DR FOUND\n");

  if(strncmp(buf_ptr->bufType, "DR" ,2) ==0)  // this is a DR (Data Rate) buffer
    {
    printf("Data Rate Buffer recd\n");

    uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    puts("set up write_req");
    uv_buf_t a[]={{.base="DR", .len=2},{.base="\n",.len=1}};
    puts("forward DR to webcontrol");
    uv_write(write_req, (uv_stream_t*) webStream, a, 2, web_write_complete);
    puts("free the DE buffer");

    DATARATEBUF *drBufptr;
    char* pDR;
    pDR = &myDataRateBuf;
 //   memcpy(pDR,buf->base,sizeof(DATARATEBUF));
    strncpy(myDataRateBuf, buf->base,nread);
    for(int i=0; i < 20; i++)
      {
      if (myDataRateBuf.dataRate[i].rateNumber == 0) break;  // we're done here
      printf("Data rate# %i, speed %i \n",myDataRateBuf.dataRate[i].rateNumber,
             myDataRateBuf.dataRate[i].rateValue);
      }

	free(buf->base);  // always release memory before exiting this callback
    free (buf_ptr);
	return;
    }
*/

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
   fprintf(stderr,"nread = %zd\n", nread);
   fprintf(stderr,"Buffer starts with: %02x %02X \n",buf->base[0], buf->base[1]);
   if (nread < 8000)  // discard this buffer for now 
	{
	fprintf(stderr, "Buffer is < 8000 bytes;  * * * IGNORE * * *\n");
	 free(buf->base);  // always release memory before exiting this callback
     free (buf_ptr);
	 return; 
	}
   if (nread > 8000)  // looks like a valid databuffer, based on length
	{
	puts("DATA BUFFER RECEIVED");
    fprintf(stderr," DataBuffer starts with: %02x %02X %02X %02X\n",
	buf->base[0], buf->base[1],buf->base[2], buf->base[3]);
	//return;
	}

////////////////////////////////////////////////////////////////////////////////////
// handle I/Q buffers coming in for storage to Digital RF

    {
    // set up memory and get a copy of the data (may be possible to eliminate this step)
  //  DATABUF *buf_ptr;
  //  buf_ptr = (DATABUF *)malloc(sizeof(DATABUF));  // allocate memory for working buf
  //  memcpy(buf_ptr, buf->base,sizeof(DATABUF));    // get data from UDP buffer
    packetCount = (long) buf_ptr->dval.bufCount;
    printf("bufcount = %ld\n", packetCount);
    int noOfChannels = buf_ptr->channelCount;
    int sampleCount = 1024 / noOfChannels;
    printf("Channel count = %i\n",buf_ptr->channelCount);

  /* local variables  for Digital RF */
    uint64_t vector_leading_edge_index = 0;
    uint64_t global_start_sample = 0;

// if bufCount is zero, it is first data packet; create the Digital RF file
// we also check buffers_received, so we can start recording even if we missed
// one or more buffers at start-up.

// TODO: NUM_SUBCHANNELS needs to be set based on actual # channels running.
// For now, just set to 1.

  global_start_sample = buf_ptr->timeStamp * (long double)SAMPLE_RATE_NUMERATOR /  
                 SAMPLE_RATE_DENOMINATOR;

  if((packetCount == 0 || buffers_received == 1) && DRFdata_object == NULL) {
    fprintf(stderr,"Create HDF5 file group, start time: %ld \n",global_start_sample);
    vector_leading_edge_index=0;
    vector_sum = 0;
    hdf_i= 0;
/*
    DRFdata_object = digital_rf_create_write_hdf5(ringbuffer_path, H5T_NATIVE_FLOAT, SUBDIR_CADENCE,
      MILLISECS_PER_FILE, global_start_sample, SAMPLE_RATE_NUMERATOR, SAMPLE_RATE_DENOMINATOR,
     "TangerineSDR", 0, 0, 1, NUM_SUBCHANNELS, 1, 1);
*/
    DRFdata_object = digital_rf_create_write_hdf5(ringbuffer_path, H5T_NATIVE_FLOAT, SUBDIR_CADENCE,
      MILLISECS_PER_FILE, global_start_sample, SAMPLE_RATE_NUMERATOR, SAMPLE_RATE_DENOMINATOR,
     "TangerineSDR", 0, 0, 1, noOfChannels, 1, 1);
      }

// here we write out DRF

  //  vector_sum = vector_leading_edge_index + hdf_i*vector_length;   // original code
    vector_sum = vector_leading_edge_index + hdf_i*sampleCount; 
/*

// in case we have to convert or otherwise interpret the DE buffer, here is a
// way to iterate through it

    puts("copy data");   // this can be eliminated by setting up buffer descrip better
    for(int j=0; j < 2048; j=j+2)
     {
     data_hdf5[j] = buf_ptr->myDataSample[j/2].I_val;
     data_hdf5[j+1] = buf_ptr->myDataSample[j/2].Q_val;
     }
*/

// debugging code for multi=subchannel support
/*
    for(int i=0; i < (sampleCount * noOfChannels); i=i+noOfChannels)
 {
  int k;
  printf("i= %i ",i);

  for(int j=0;j<noOfChannels;j++) 
   {
   k = j + i;
//   printf("k=%i   \n",k);
   printf("%f %f",buf_ptr->theDataSample[k].I_val,buf_ptr->theDataSample[k].Q_val);
   }
   printf("\n");
   
 }
*/

    fprintf(stderr,"Write HDF5 data to %s \n", ringbuffer_path);
// push buffer directly to DRF just like it is
    if(DRFdata_object != NULL)  // make sure there is an open DRF file
	  {
  //    result = digital_rf_write_hdf5(DRFdata_object, vector_sum, buf_ptr->theDataSample,vector_length);
      result = digital_rf_write_hdf5(DRFdata_object, vector_sum, buf_ptr->theDataSample,sampleCount) ;
	  fprintf(stderr,"DRF write result = %d, vector_sum = %ld \n",result, vector_sum);
	  }
       
  //  result = digital_rf_write_hdf5(data_object, vector_sum, data_hdf5,vector_length); 

    hdf_i++;  // increment count of hdf buffers processed

    free (buf_ptr);  // free the work buffer
    }
  }

  free(buf->base);  // free the callback buffer
  return;  // end of callback for handling incoming I/Q data
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
    return;
	}

////////////////////////////////////////////////////////////////////
  if(strncmp(mybuf, CONFIG_CHANNELS, 2)==0) 
    {
//    CHANNELBUF *channelBuf_ptr;
//channelBuf_ptr = (CHANNELBUF *)malloc(sizeof(CHANNELBUF));  // TODO: free this later
    uv_udp_send_t send_req;
    char b[400];
    printf("Config channels (CH) received\n");
	memcpy(h.channelBuffer.chCommand, CONFIG_CHANNELS, sizeof(CONFIG_CHANNELS));  // Put the command into buf
    const char comma[2] = ",";
    char *token;
    token = strtok(mybuf, comma);
    printf("initial token = %s\n", token);
   // token = strtok(NULL, comma);

    for (int i=0; i < 16; i++)
      {
      printf("Channel# %i :\n",i);
      token = strtok(NULL, comma);
//printf("next token = %s\n", token);
      int ret = sscanf(token,"%i",&h.channelBuffer.channelDef[i].channelNo );
      printf("converted to %i \n",h.channelBuffer.channelDef[i].channelNo);
      token = strtok(NULL, comma);
      printf("next token = %s\n", token);
      if(strcmp(token,"Off")==0)
		h.channelBuffer.channelDef[i].antennaPort = -1;
	  else
        ret = sscanf(token,"%i",&h.channelBuffer.channelDef[i].antennaPort);
      printf("converted to %i \n",h.channelBuffer.channelDef[i].antennaPort);
      token = strtok(NULL, comma);
 //     printf("next token = %s\n", token);
      ret = sscanf(token,"%lf",&h.channelBuffer.channelDef[i].channelFreq);
 //     printf("converted to %lf \n",h.channelBuffer.channelDef[i].channelFreq);
      token = strtok(NULL, comma);
 //     printf("next token = %s\n", token);
      ret = sscanf(token,"%lf",&h.channelBuffer.channelDef[i].channelBandwidth);
 //     printf("converted to %lf \n",h.channelBuffer.channelDef[i].channelBandwidth);

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

    uv_udp_send_t send_req;

	char b[60];
	for(int i=0; i< 60; i++) { b[i] = 0; }

	strcpy(b, START_DATA_COLL );
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
    int result = digital_rf_close_write_hdf5(DRFdata_object);
    DRFdata_object = NULL;
    fprintf(stderr,"DRF close, result = %d \n",result);
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
//  puts("process_command triggered; set up uv_write_t");
//  uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
//  puts("set up write_req");
//  uv_buf_t a[]={{.base=mybuf,.len=nread},{.base="\n",.len=3}};
// forward command to DE
//  puts("write to DE");


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
      puts("received UDP zero");
      free(buf->base);
	  return;
    }
  printf("UDP data recvd, bytes = %ld\n", nread);

  DATABUF *buf_ptr;
  buf_ptr = (DATABUF *)malloc(sizeof(DATABUF));  // allocate memory for working buf
// TODO: ensure there is a free for this memory before every return
  memcpy(buf_ptr, buf->base,nread);    // get data from UDP buffer

  printf("DE BUFTYPE = %s \n",buf_ptr->bufType);

  // respond to STATUS INQUIRY

  if(strncmp(buf_ptr->bufType, "OK" ,2) ==0)
	{
	puts("OK status message received from DE!  It's alive!!");
    uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    puts("set up write_req");
    uv_buf_t a[]={{.base="OK", .len=2},{.base="\n",.len=1}};
    puts("Send OK status to webcontrol");
    uv_write(write_req, (uv_stream_t*) webStream, a, 2, web_write_complete);
    return;
	}

  if(strncmp(buf_ptr->bufType, DATARATE_RESPONSE, 2)==0)
	{
  DATARATEBUF *rbuf_ptr;
  rbuf_ptr = (DATARATEBUF *)malloc(sizeof(DATARATEBUF));  // allocate memory for working buf

    COMBOBUF cbuf;
   // *cbuf cbufp;

// TODO: ensure there is a free for this memory before every return
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
  printf("intial entry %i %i \n",cbuf.dbuf.dataRate[0].rateNumber, cbuf.dbuf.dataRate[0].rateValue);


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
    return;
	}

  if(strncmp(buf_ptr->bufType, "AK" ,2) ==0)
    {
    printf("AK buffer contains %x %x %x %x %x %x \n",
           buf->base[0], buf->base[1], buf->base[2],
			buf->base[3], buf->base[4], buf->base[4]);

/////////////////////////////////////////////////
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
	puts("DATA BUFFER RECEIVED");
    fprintf(stderr," DataBuffer starts with: %02x %02X %02X %02X\n",
	buf->base[0], buf->base[1],buf->base[2], buf->base[3]);
	//return;
	}

////////////////////////////////////////////////////////////////////////////////////
// handle I/Q buffers coming in for storage to Digital RF

    {
    // set up memory and get a copy of the data (may be possible to eliminate this step)
  //  DATABUF *buf_ptr;
  //  buf_ptr = (DATABUF *)malloc(sizeof(DATABUF));  // allocate memory for working buf
  //  memcpy(buf_ptr, buf->base,sizeof(DATABUF));    // get data from UDP buffer
    packetCount = (long) buf_ptr->dval.bufCount;
    printf("bufcount = %ld\n", packetCount);
    printf("Channel count = %i \n",buf_ptr->channelCount);
  /* local variables  for Digital RF */
    uint64_t vector_leading_edge_index = 0;
    uint64_t global_start_sample = 0;

// if bufCount is zero, it is first data packet; create the Digital RF file
// we also check buffers_received, so we can start recording even if we missed
// one or more buffers at start-up.

// TODO: NUM_SUBCHANNELS needs to be set based on actual # channels running.
// For now, just set to 1.

  global_start_sample = buf_ptr->timeStamp * (long double)SAMPLE_RATE_NUMERATOR /  
                 SAMPLE_RATE_DENOMINATOR;

  if((packetCount == 0 || buffers_received == 1) && DRFdata_object == NULL) {
    fprintf(stderr,"Create HDF5 file group, start time: %ld \n",global_start_sample);
    vector_leading_edge_index=0;
    vector_sum = 0;
    hdf_i= 0;
 //   NUM_SUBCHANNELS = buf_ptr->channelCount;
/*
    DRFdata_object = digital_rf_create_write_hdf5(ringbuffer_path, H5T_NATIVE_FLOAT, SUBDIR_CADENCE,
      MILLISECS_PER_FILE, global_start_sample, SAMPLE_RATE_NUMERATOR, SAMPLE_RATE_DENOMINATOR,
     "TangerineSDR", 0, 0, 1, NUM_SUBCHANNELS, 1, 1);

*/    DRFdata_object = digital_rf_create_write_hdf5(ringbuffer_path, H5T_NATIVE_FLOAT, SUBDIR_CADENCE,
      MILLISECS_PER_FILE, global_start_sample, SAMPLE_RATE_NUMERATOR, SAMPLE_RATE_DENOMINATOR,
     "TangerineSDR", 0, 0, 1,buf_ptr->channelCount , 1, 1);
      }

// here we write out DRF

    vector_sum = vector_leading_edge_index + hdf_i*vector_length; 
/*

// in case we have to convert or otherwise interpret the DE buffer, here is a
// way to iterate through it

    puts("copy data");   // this can be eliminated by setting up buffer descrip better
    for(int j=0; j < 2048; j=j+2)
     {
     data_hdf5[j] = buf_ptr->myDataSample[j/2].I_val;
     data_hdf5[j+1] = buf_ptr->myDataSample[j/2].Q_val;
     }
*/

    fprintf(stderr,"Write HDF5 data to %s \n", ringbuffer_path);
// push buffer directly to DRF just like it is
    if(DRFdata_object != NULL)  // make sure there is an open DRF file
	  {
      result = digital_rf_write_hdf5(DRFdata_object, vector_sum, buf_ptr->theDataSample,vector_length);
	  fprintf(stderr,"DRF write result = %d, vector_sum = %ld \n",result, vector_sum);
	  }
       
  //  result = digital_rf_write_hdf5(data_object, vector_sum, data_hdf5,vector_length); 

    hdf_i++;

    free (buf_ptr);  // free the work buffer
    }

  free(buf->base);

  }
 ////////////// reads config items from the (python) config file /////////
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
//  printf("line length %zu: ",read);
//  printf("%s \n",line);
  cp = strdup(line);  // allocate enuff memory for a copy of this
//  printf("cp=%s\n",cp);
  token = strtok(cp, delimiters);
//  printf("first token='%s'\n",token);
  if(strcmp(arg,token) == 0)
   {
  token = strtok(NULL, delimiters);
//  printf("second token=%s\n",token);
  printf("config value found = '%s', length = %lu\n",token,strlen(token));
  
  strncpy(result,token,strlen(token)-1);
  result[strlen(token)-1] = 0x00;  // terminate the string
  free(cp);
  return(1);
   }
  }
  free(cp);
  return(0);
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
   	  if ( (NULL == CU_add_test(pSuite, "max_test_1", max_test_1)) ||
          (NULL == CU_add_test(pSuite, "max_test_2", max_test_2))  ||
          (NULL == CU_add_test(pSuite, "max_test_3", max_test_3))  ||
		  (NULL == CU_add_test(pSuite, "config_test1", test_get_config))
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

// get the configuration file
  config_t cfg;
  config_setting_t *setting;
 // char *DE_ip_str;
  char DE_ip_str[20];
  packetCount = 0;
  int DE_port;
  int controller_port;

  config_init(&cfg);

  /* Read the file. If there is an error, report it and exit. */

// The only thing we use this config file for is to get the path to the
// python config file. Seems like a kludge, but allows flexibility in
// system directory structure.

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

  char result[100];
  char target[30];
 // strcpy(target,"configport");
  int num_items = 0;
  puts("start");
 // printf("looking for '%s'\n",target);
  num_items = rconfig("configport",result,0);
  if(num_items == 0)
    {
    printf("ERROR - configport setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT = '%s'\n",result);
 //   printf("len =%lu\n",strlen(result));
    LH_CONF_IN_port = atoi(result);
    num_items = rconfig("dataport",result,0);
    LH_DATA_IN_port = atoi(result);
    }

//  strcpy(target,"DE_ip");
//  int num_items = 0;
  puts("start");
 // printf("looking for '%s'\n",target);
  num_items = rconfig("DE_ip",result,0);
  if(num_items == 0)
    {
    printf("ERROR - DE_ip setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT for DE_ip = '%s'\n",result);
    printf("len =%lu\n",strlen(result));
    strcpy(DE_ip_str, result);
    }

 // strcpy(target,"DE_port");
//  puts("start");
 // printf("looking for '%s'\n",target);

  num_items = rconfig("DE_port",result,0);
  if(num_items == 0)
    {
    printf("ERROR - DE_ip setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT for DE_port = '%s'\n",result);
    printf("len =%lu\n",strlen(result));
    DE_port = atoi(result);
    }

 // strcpy(target,"controlport");

  num_items = rconfig("controlport",result,0);
  if(num_items == 0)
    {
    printf("ERROR - controlport setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT = '%s'\n",result);
    printf("len =%lu\n",strlen(result));
    controller_port = atoi(result);
    }


 // strcpy(target,"ringbuffer_path");
  num_items = rconfig("ringbuffer_path",result,0);
  if(num_items == 0)
    {
    printf("ERROR - ringbuffer_path setting not found in config.ini");
    }
  else
    {
    printf(" CONFIG RESULT = '%s'\n",result);
    printf("len =%lu\n",strlen(result));
    strcpy(ringbuffer_path, result);
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



	


////////////////////////////////////////////////////////////////


  return uv_run(loop, UV_RUN_DEFAULT);
}
