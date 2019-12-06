#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <string.h>
#include "de_signals.h"
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>


#define PORT 6200  // temporary

uv_loop_t *loop;
//uv_udp_t send_socket;
//uv_udp_t recv_socket;
//uv_sem_t UDP_send_complete ;
sem_t mutex;
pthread_t t1;

int sockfd;  // socket definition for UDP
struct sockaddr_in     servaddr; 

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
	  I = A * (sin ( (float)i / 57.295778666));
	  Q = A * (cos ( (float)i / 57.295778666));

	  myBuffer.timeStamp = (double) epoch;
	  myBuffer.myDataSample[i].I_val = I;
	  myBuffer.myDataSample[i].Q_val = Q;
           }

  char *hello = "Hello from DE"; 
  int sentBytes;
  long loopstart;
  loopstart = clock();

  for(int j=0; j<10; j++)
  { 
   // puts("UDP thread start; hit sem_wait");
    sem_wait(&mutex);
   // puts("passed wait");
    myBuffer.bufcount = bufcount++;
    sentBytes = sendto(sockfd, (const struct dataBuf *) &myBuffer, sizeof(myBuffer),
	MSG_CONFIRM, (const struct sockaddr *) &servaddr,  
            sizeof(servaddr));
    fprintf(stderr,"UDP message sent from thread. bytes= %d\n", sentBytes); 
  //  sleep(1);
    usleep(5288);  // wait for this many microseconds
    puts("UDP thread end");
    sem_post(&mutex);
  }

  printf("sending data took ~ %zd  microsec\n", clock() - loopstart);

  puts("ending thread");
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

int main() {
  puts("starting");
  loop = uv_default_loop();

  uv_tcp_t server;
  uv_tcp_init(loop, &server);
  //uv_udp_init(loop, &send_socket);


  puts("define UDP socket");


  // Creating socket file descriptor 
  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
      }
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 

    struct hostent* hptr = gethostbyname("192.168.1.75");
    if(!hptr) puts ("gethostbyname error");
    if(!hptr->h_addrtype != AF_INET)
	puts("bad address family");


    servaddr.sin_addr.s_addr =     //INADDR_ANY; 
	((struct in_addr*) hptr->h_addr_list[0])->s_addr;


  struct sockaddr_in bind_addr;
  uv_ip4_addr("0.0.0.0", 7000, &bind_addr);
  puts("bind");
  uv_tcp_bind(&server, (struct sockaddr *)&bind_addr, 0);
  puts("start to listen");
  int r = uv_listen((uv_stream_t*) &server, 128, on_new_connection);
  if (r) {
    fprintf(stderr, "Listen error!\n");
    return 1;
  }
  puts("start loop");
  return uv_run(loop, UV_RUN_DEFAULT);
}
