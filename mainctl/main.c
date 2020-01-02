/* Copyright (C) 2019 The University of Alabama
* Author: William (Bill) Engelke, AB4EJ
* With funding from the Center for Advanced Public Safety and
* The National Science Foundation.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
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

extern void UDPdiscover();

#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <string.h>
#include <libconfig.h>
#include "digital_rf.h"
#include <dirent.h>
#include <errno.h>
#include "de_signals.h"

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
static	int hdf_i = 0; 
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
static  char hdf5File[50] = "/media/odroid/hamsci/hdf5";
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
const char *ringbufferPath;
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

// the following probably not necessart  (TODO)
static uv_tcp_t* websocket; // socket to be used for comm to web server
//static uv_stream_t* websockethandle;

static uv_stream_t* webStream;


static long packetCount;

// buffer for A/D data from DE
struct dataSample
	{
	float I_val;
	float Q_val;
	};
typedef struct databBuf
	{
	long bufCount;
	long timeStamp;
	//struct dataSample myDataSample[1024]; this is the logical layout using dataSample.
    //    Below is what Digital RF reequires to be able to understand the samples.
    //    In the array, starting at zero, sample[j] = I, sample[j+1] = Q (complex data)
        float theDataSample[2048];  // should be double the number of samples
	} DATABUF ;


static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

//// *********************** Start of Code  *******************************//////

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

/////////////////////////////////////////////////////////////////////
// callback for when a command is received from webcontroller
/////////////////////////////////////////////////////////////////

void process_command(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
  puts("process_command routine triggered");
  if(nread == -4095)  // TODO: this seems to be junk coming from flask app (?) - need to fix
	{ puts("ignore 1 buffer"); return;
	}
  if (nread < 0) {
    fprintf(stderr, "Webcontroller Read error, nread = %ld\n",nread);  // DE disconnected/crashed
    {  // inform webcontrol that DE seems unresponsive
     uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
     puts("set up write_req");
     uv_buf_t a[]={{.base="NAK", .len=2},{.base="\n",.len=1}};
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
  puts("Command received from web control");
  printf("nread = %zd\n",nread);
  char mybuf[80];
  memset(&mybuf, 0, sizeof(mybuf));
  strncpy(mybuf, buf->base, nread-1);   // subtract 1 to strip CR
// NOTE! if controller does not send \n at end of buffer, commmand will be truncated (above)
  puts("mybuf="); puts(mybuf);
  if(strncmp(buf->base,"BYE",3)==0)
    {
    puts("halting");
    uv_stop(loop);
    }
  if(strncmp(buf->base, START_DATA_COLL,2)==0)  // is this a command to start collecting data?
    {
      buffers_received = 0;  // this lets us count buffers independently of counter in the buffer itself
    }
 
//TODO: if this is executed without a DRF file being open, it crashes program. Need to trap or otherwise detect

// try to close DRF file, but only if one is open
  puts("check if DRF file open; if so, close it");
  if(strncmp(buf->base, STOP_DATA_COLL,2)==0 && DRFdata_object != NULL)
	{
      puts("Closing DRF file");
	  result = digital_rf_close_write_hdf5(DRFdata_object);
      DRFdata_object = NULL;
	}
  puts("process_command triggered; set up uv_write_t");
  uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
  puts("set up write_req");
  uv_buf_t a[]={{.base=mybuf,.len=nread},{.base="\n",.len=1}};
// forward command to DE
  puts("write to DE");
  uv_write(write_req, DEsockethandle, a, 1, DE_write_cb);
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

///////////////////////////////////////////////////////////////
//  Callback for when UDP data packets received from DE 
//////////////////////////////////////////////////////////////
void on_UDP_read(uv_udp_t * recv_handle, ssize_t nread, const uv_buf_t * buf,
		const struct sockaddr * addr, unsigned flags)
  {
  if(nread == 0 )
    { 
      puts("received UDP zero");
      free(buf->base);
	  return;
    }
  buffers_received++;
  //fprintf(stderr,"received UDP packet# %zd\n",packetCount);
  fprintf(stderr,"nread = %zd\n", nread);
  if (nread > 8000)  // looks like a valid databuffer, based on length
    {
    DATABUF *buf_ptr;

    buf_ptr = (DATABUF *)malloc(sizeof(DATABUF));  // allocate memory for working buf
    memcpy(buf_ptr, buf->base,sizeof(DATABUF));    // get data from UDP buffer
    long packetCount = (long) buf_ptr->bufCount;
    printf("bufcount = %ld\n", packetCount);

  /* local variables  for Digital RF */
    uint64_t vector_leading_edge_index = 0;

    uint64_t global_start_sample = 0;

// if bufCount is zero, it is first data packet; create the Digital RF file
// we also check buffers_received, so we can start recording even if we missed
// one or more buffers at start-up.

// TODO: NUM_SUBCHANNELS needs to be set based on actual # channels running

  global_start_sample = buf_ptr->timeStamp * (long double)SAMPLE_RATE_NUMERATOR /  
                 SAMPLE_RATE_DENOMINATOR;

  if((packetCount == 0 || buffers_received == 1) && DRFdata_object == NULL) {
    fprintf(stderr,"Create HDF5 file group, start time: %ld \n",global_start_sample);
    vector_leading_edge_index=0;
    DRFdata_object = digital_rf_create_write_hdf5(ringbuffer_path, H5T_NATIVE_FLOAT, SUBDIR_CADENCE,
      MILLISECS_PER_FILE, global_start_sample, SAMPLE_RATE_NUMERATOR, SAMPLE_RATE_DENOMINATOR,
     "TangerineSDR", 0, 0, 1, NUM_SUBCHANNELS, 1, 1);
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
      result = digital_rf_write_hdf5(DRFdata_object, vector_sum, buf_ptr->theDataSample,vector_length); 
  //  result = digital_rf_write_hdf5(data_object, vector_sum, data_hdf5,vector_length); 

    hdf_i++;

    free (buf_ptr);  // free the work buffer
    }


  free(buf->base);

  //uv_udp_recv_stop(recv_handle);
  }



/*  ************************************************************ */
int main() {
  puts("starting");
  puts("UDPdiscovery    ******    *****");
  //system("pwd");
  int retval = system("./discover");
  fprintf(stderr,"Discover return= %d\n",retval);

// get the configuratin file
  config_t cfg;
  config_setting_t *setting;
  const char *DE_ip_str;
  packetCount = 0;
  int DE_port;
  int controller_port;

  config_init(&cfg);

  /* Read the file. If there is an error, report it and exit. */
  if(! config_read_file(&cfg, "/home/odroid/projects/TangerineSDR-notes/mainctl/main.cfg"))
  {
    fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
            config_error_line(&cfg), config_error_text(&cfg));
    puts("ERROR - there is a problem with main.cfg configuration file");
    config_destroy(&cfg);
    return(EXIT_FAILURE);
  }

  if(config_lookup_string(&cfg, "DE_ip", &DE_ip_str))
    printf("Setting ip address of DE to: %s\n\n", DE_ip_str);
  else
    fprintf(stderr, "No 'name' setting in configuration file.\n");

  if(config_lookup_int(&cfg, "DE_port", &DE_port))
	fprintf(stderr,"Setting DE port to: %d\n",DE_port);
  else
	fprintf(stderr,"No DE_port setting in configuration file\n");

  if(config_lookup_int(&cfg, "controller_port", &controller_port))
    fprintf(stderr,"Will listen on port %d for commands from webcontrol\n", controller_port);
  else
    fprintf(stderr,"No port set for listening to webcontrol\n");

  if(config_lookup_string(&cfg, "ringbuffer_path", &ringbufferPath))
    {
    strcpy(ringbuffer_path, ringbufferPath);
    fprintf(stderr,"Will store Digital RF / HDF5 files in %s \n", ringbuffer_path);
    }
  else
    fprintf(stderr,"No port set for listening to webcontrol\n");


  loop = uv_default_loop();



// set up to listen to incoming port for commands from web controller
  uv_tcp_t server;
  uv_tcp_init(loop, &server);
  struct sockaddr_in bind_addr;
  uv_ip4_addr("0.0.0.0", 6100, &bind_addr);
  puts("Bind to input (terminal) port for listening");
  uv_tcp_bind(&server, (struct sockaddr *)&bind_addr, 0);
  int r = uv_listen((uv_stream_t*) &server, 128, on_new_connection);
  if (r) {
    fprintf(stderr, "Listen error!\n");
    return 1;
    }
  
//      Connect to DE as client, using TCP

// NOTE. If DE expects to be "discovered" a la HPSDR, this will have to be 
// replaced with discovery code.  See pihpsdr old_protocol_N1.c

  puts("Trying to connect to DE as client");
  uv_tcp_t* socket = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, socket);
  uv_connect_t* connect = (uv_connect_t*)malloc(sizeof(uv_connect_t));
  struct sockaddr_in dest;
  uv_ip4_addr(DE_ip_str, DE_port, &dest); 
  uv_tcp_connect(connect, socket, (const struct sockaddr*)&dest, on_DE_CL_connect);

  puts("prep to receive UDP data");
  uv_udp_t recv_socket;
  uv_udp_init(loop, &recv_socket);
  struct sockaddr_in recv_addr;
  uv_ip4_addr("0.0.0.0", 1024, &recv_addr);
  uv_udp_bind(&recv_socket, (const struct sockaddr *)&recv_addr, UV_UDP_REUSEADDR);
  uv_udp_recv_start(&recv_socket, alloc_buffer, on_UDP_read);


  puts("DE connection request done; awaiting response from DE..."); 

/////////// Digital RF Setup //////////////////////////////////////

  /* local variables */

/*
  uint64_t vector_leading_edge_index = 0;
  uint64_t global_start_sample = (uint64_t)(START_TIMESTAMP * ((long double)SAMPLE_RATE_NUMERATOR)/SAMPLE_RATE_DENOMINATOR);

*/


  //strcpy(path_to_DRF_data, "/media/odroid/hamsci/hdf5");    // TODO: replace with config item
  DIR* dir = opendir(ringbufferPath);
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


// do UDP discovery
 // puts("trying UDP discovery ********************");
 // UDPdiscover();

/*
	if(fp)
		{
		char buffer[128];
		while (!feof(fp))
			{
			if(fgets(buffer, 128, fp) != NULL) {}
			}
		pclose(fp);
		buffer[strlen(buffer)-1] = '\0';
		char theResult[80];
		strcpy(theResult, buffer);
		printf("result of mkdir = %s",theResult);
		}

*/



	


////////////////////////////////////////////////////////////////


  return uv_run(loop, UV_RUN_DEFAULT);
}
