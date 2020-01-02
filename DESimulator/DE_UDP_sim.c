#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <uv.h>
#include <string.h>
#include<arpa/inet.h>

#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include "de_signals.h"

#define PORT 1024  // temporary  (mainctl may expect 6200)
#define BUFFERLEN 65	// Maximum length of buffer
// #define PORT 1024	// Port to watch

uv_loop_t *loop;
uv_udp_t send_socket;
uv_udp_t recv_socket;
//uv_sem_t UDP_send_complete ;
sem_t mutex;
pthread_t t1;

int sockfd;  // socket definition for UDP
struct sockaddr_in     servaddr; 

struct sockaddr_in si_DE, si_main;


//char *theIP;
//extern void UDPhandshake();

// notional buffer for DE A/D output. Will update with the real thing later...
struct dataSample
	{
	float I_val;
	float Q_val;
	};

struct dataBuf
	{
	long bufcount;
	long timeStamp;
	struct dataSample myDataSample[1024];
	};

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

void echo_write(uv_write_t *req, int status) {
  if (status == -1) {
    fprintf(stderr, "Write error!\n");
  }
  char *base = (char*) req->data;

  puts("free req");
  free(req);
}

void* UDPthread(void* arg)
{
  puts("starting thread");
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

	for (int i = 0; i < 1024; i++) {
	  I =  sin ( (double)i * 2.0 * 3.1415926535897932384626433832795 / 1024.0);
	  Q =  cos ( (double)i * 2.0 * 3.1415926535897932384626433832795 / 1024.0);
	  myBuffer.timeStamp = (double) epoch;
	  myBuffer.myDataSample[i].I_val = I;
	  myBuffer.myDataSample[i].Q_val = Q;
           }

  char *hello = "Hello from DE"; 
  int sentBytes;
  long loopstart;
  loopstart = clock();

  while(1)
  { 
   // puts("UDP thread start; hit sem_wait");
    sem_wait(&mutex);
   // puts("passed wait");
    myBuffer.bufcount = bufcount++;
    sentBytes = sendto(sockfd, (const struct dataBuf *) &myBuffer, 		   	sizeof(myBuffer),
	MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
            sizeof(servaddr));
    fprintf(stderr,"UDP message sent from thread. bytes= %d\n", sentBytes); 
   sleep(1);
    usleep(528);  // wait for this many microseconds
    puts("UDP thread end");
    sem_post(&mutex);
  }

  printf("sending data took ~ %zd  microsec\n", clock() - loopstart);

  puts("ending thread");
}

void on_UDP_send(uv_udp_send_t* req, int status)
{
  printf("UDP send complete, status = %d", status);
  //free(req);
  return;
}



void on_UDP_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) { 
    //const uv_buf_t replybuf;
    printf("UDP traffic detected\n");
    if (nread < 0) {
        fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) req, NULL);
        free(buf->base);
        return;
    }
    int sentBytes;
    char sender[17] = { 0 };
    char replybuf[60];
   // memset(&replybuf,0, sizeof(replybuf));
    uv_ip4_name((const struct sockaddr_in*) addr, sender, 16);
    fprintf(stderr, "Recv from %s\n", sender);
    for(int i=0;i<nread;i++){fprintf(stderr,"%02X ",buf->base[i]);}
    fprintf(stderr,"\n");
    if((buf->base[0] & 0xFF) == 0xEF && (buf->base[1] & 0xFF) == 0xFE) {
	fprintf(stderr,"discovery packet detected\n");
	//fprintf(stderr, "msg len = %ld \n", discover_msg.len);
	uv_udp_send_t send_req;
 
 	puts("create discovery reply buf");

	char b[60];
	for(int i=0;i<60;i++) {b[i] = 0;}
	b[0] = 0xEF;
	b[1] = 0xFE;
	b[2] = 0x02;
	uv_buf_t a[]={{.base = b, .len=60}};

	for(int i=0;i<60;i++){fprintf(stderr,"%02X ",a[0].base[i]);}; 
	fprintf(stderr,"\n");

	puts("set length");
	//discover_msg.len = sizeof(replybuf);
    	struct sockaddr_in send_addr;
	puts("set send_addr");
    //	uv_ip4_addr("255.255.255.255", 1024, &send_addr);
   // 	uv_ip4_addr("192.168.1.75", 1024, &send_addr);
	//puts("send");

// original discovery-reply code

 // struct sockaddr_in si_DE, si_main;
 // *si_main = *addr;	// try to get input on this
  int s, i, slen = sizeof(si_main) , recv_len; 
	 slen = 63;
		//printf("Received packet from %s:%d\n", inet_ntoa(si_main.sin_addr), 			//	ntohs(si_main.sin_port));
		//printf("Received packet from %s\n", addr); 				//ntohs(si_main.sin_port));
		//char theotherIP[16];
		//strcpy(sender, inet_ntoa(si_main.sin_addr));
		//printf("Data: %s\n" , buf);
		printf("IP addr: %s \n",sender);		
		//now reply to the client with the same data
		printf("Received packet from %s:%d\n", inet_ntoa(si_main.sin_addr), 				ntohs(si_main.sin_port));
/*
		if (sendto(s, buf->base, nread, 0, (struct sockaddr*) &si_main, slen) == -1)
		
		{
			puts("ERROR - sendto() failed");
		}
*/


	fprintf(stderr,"sentBytes = %d \n", sentBytes);
	puts("send done  (?)");
     }


    free(buf->base);
    uv_udp_recv_stop(req);
}


///////////////////////// Command Processing /////////////////////////////

void handle_command(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
  if (nread < 0) {
    fprintf(stderr, "Read error!\n");
    puts("do uv_close");
    uv_close((uv_handle_t*)client, NULL);
    puts("free read buffer");
    free(buf->base);  // release memory allocated to read buffer
    return ;
  }
  printf("nread = %zd\n",nread);

  char mybuf[80];
  memset(&mybuf, 0, sizeof(mybuf));
  strncpy(mybuf, buf->base, nread);
  puts(mybuf);
// handle command coming from mainctl process
  if(strncmp(buf->base,"BYE",3)==0)
    {
    puts("halting");
    uv_stop(loop);
    }

  if(strncmp(buf->base, STATUS_INQUIRY, 2) == 0)
    {
    puts("STATUS INQUIRY RECD");
    puts("set up write_req");
    uv_buf_t a[] = { { .base = "OK", .len = 2 }, {.base = "K", .len=1}};

    uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
  //  write_req->data = (void*)buf->base;
    puts("reply 'OK'");
    uv_write(write_req, client, a , 1, echo_write);
    }

 ////////////// start data collection command received ///////////////////
  if(strncmp(buf->base, START_DATA_COLL, 2) == 0)
    {
    puts("Try to create thread");
    sem_post(&mutex);  // just in case this semaphore is locked
// above: what if we get 2 SC commands in a row? May create confusion
    pthread_create(&t1,NULL,UDPthread,NULL);
    sem_destroy(&mutex);
    char *hello = "Hello from DE, after thread start"; 
    int sentBytes;
    sentBytes = sendto(sockfd, (const char *)hello, strlen(hello), 
        MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
            sizeof(servaddr)); 
    fprintf(stderr,"Hello message sent. bytes= %d\n", sentBytes); 
    puts("send complete");
    }

  if(strncmp(buf->base, STOP_DATA_COLL, 2) == 0)
  {
    pthread_cancel(t1);
  }
}


void on_new_connection(uv_stream_t *server, int status) {
  if (status == -1) {
    puts("negative connect status");
    return;
  }
  puts("new connection from main.");
  uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, client);
  if (uv_accept(server, (uv_stream_t*) client) == 0) {
    uv_read_start((uv_stream_t*) client, alloc_buffer, handle_command);
  }
  else {
    uv_close((uv_handle_t*) client, NULL);
  }
}

void crash(char *s)
{
	perror(s);
	exit(1);
}

int main() {
  puts("starting");
  loop = uv_default_loop();
//  char buf[63];
  uv_tcp_t server;
  uv_tcp_init(loop, &server);

// try to connect to mainctl
//  struct sockaddr_in si_me, si_other;
//  memset((char *) &si_me, 0, sizeof(si_me));
//  memset((char *) &si_other, 0, sizeof(si_other));
  puts("wait for UDP handshake\n");
  
  bool connected = 0;
 // struct sockaddr_in si_DE, si_main;	
  int s, i, slen = sizeof(si_main) , recv_len;  
  char buf[BUFFERLEN];	
  //create UDP socket
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
		crash("ERROR. could not create socket ");
  }
	
  // clear structure
  memset((char *) &si_DE, 0, sizeof(si_DE));
	
  si_DE.sin_family = AF_INET;
  si_DE.sin_port = htons(PORT);
  si_DE.sin_addr.s_addr = htonl(INADDR_ANY);
	
  // now bind the socket to the port
  if( bind(s , (struct sockaddr*)&si_DE, sizeof(si_DE) ) == -1)
  {
		crash("ERROR. could not bind socket to port");
  }	
  // process incoming UDP packets, looking for discovery packet
  while(1)
	{
		printf("Watching for discovery...");
		fflush(stdout);
		
  // await data  (blocking call)
		if ((recv_len = recvfrom(s, buf, BUFFERLEN, 0, (struct sockaddr *) &si_main, &slen)) == -1)
		{
			crash("ERROR - recvfrom() failed");
		}
  // a discovery packet starts with hex EFFE02 (using only rightmost byte of each word)
		if((buf[0] & 0xFF) == 0xEF && (buf[1] & 0xFF) == 0xFE
			&& (buf[2] & 0xFF) == 0x02 ) 
		  {
		  fprintf(stderr,"Received SDR handshake!\n");
		  connected = 1;
		  }
		else
		  continue;  // discard packets of all other kinds

  // log source of packet
		printf("Received packet from %s:%d\n", inet_ntoa(si_main.sin_addr), 				ntohs(si_main.sin_port));
		char theotherIP[16];
		strcpy(theotherIP, inet_ntoa(si_main.sin_addr));
		printf("Data: %s\n" , buf);
		printf("IP addr: %s \n",theotherIP);		
		//now reply to the client with the same data

		if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_main, slen) == -1)
		{
			crash("ERROR - sendto() failed");
		}

  // here we pass the client's IP back to the caller via pointer
		//*theIP = theotherIP;
		//printf("theIP = %s \n", *theIP);
		if(connected) {
		  close(s); 
		 break;}
	}

	close(s);
	//return ;


/*
  UDPhandshake(&theIP);
  
  printf("Received discovery packet from %s \n", theIP);
  free(theIP);
  puts("define UDP socket for sending discovery response");
// based on code in discovery section
  struct sockaddr_in si_main;
  int s, i, slen = sizeof(si_main);
  puts("create socket");
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
		printf("ERROR. could not create socket \n");
    }
  // now bind the socket to the port
  puts("bind socket to port");
  if( bind(s , (struct sockaddr*)&si_main, sizeof(si_main) ) == -1)
	{
		printf("ERROR. could not bind socket to port\n");
	}
  memset (buf, 0, sizeof(buf));
  buf[0] = 0xEF; buf[1] = 0xFE; buf[2] = 0x02;
  puts("send");
  if (sendto(s, buf, 63, 0, (struct sockaddr*) &si_main, slen) == -1)
	{
		printf("ERROR - sendto() failed\n");
	}
	
*/
  // Creating socket file descriptor for UDP
  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
      }
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 

    struct hostent* hptr = gethostbyname("192.168.1.90");
    if(!hptr) puts ("gethostbyname error");
    if(!hptr->h_addrtype != AF_INET)
	puts("bad address family");

    servaddr.sin_addr.s_addr =     //INADDR_ANY; 
	((struct in_addr*) hptr->h_addr_list[0])->s_addr;


  struct sockaddr_in bind_addr;
  uv_ip4_addr("0.0.0.0", 7000, &bind_addr);
  puts("bind");
  uv_tcp_bind(&server, (struct sockaddr *)&bind_addr, 0);
  puts("start to listen for TCP");
  int r = uv_listen((uv_stream_t*) &server, 128, on_new_connection);
  if (r) {
    fprintf(stderr, "Listen error!\n");
    return 1;
  }
    puts("set up for UDP receiving");
    uv_udp_init(loop, &recv_socket);
    struct sockaddr_in recv_addr;
    uv_ip4_addr("0.0.0.0", 1024, &recv_addr);
    uv_udp_bind(&recv_socket, (const struct sockaddr *)&recv_addr, UV_UDP_REUSEADDR);
    uv_udp_recv_start(&recv_socket, alloc_buffer, on_UDP_read);

    puts("set up for UDP sending");
    uv_udp_init(loop, &send_socket);
    struct sockaddr_in broadcast_addr;
    uv_ip4_addr("0.0.0.0", 0, &broadcast_addr);
    uv_udp_bind(&send_socket, (const struct sockaddr *)&broadcast_addr, 0);
    uv_udp_set_broadcast(&send_socket, 1);

  puts("start loop");
  return uv_run(loop, UV_RUN_DEFAULT);
}
