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
struct sockaddr_in SBCaddr;

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

struct sockaddr_in si_DE, si_main;

void on_UDP_send(uv_udp_send_t* req, int status)
{
  printf("UDP send complete, status = %d\n", status);
  //free(req);
  return;
}

void on_UDP_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) {
    puts("UDP data detected");
    if (nread < 0) {
        fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) req, NULL);
        free(buf->base);
        return;
    }

    char sender[17] = { 0 };
    uv_ip4_name((const struct sockaddr_in*) addr, sender, 16);
    fprintf(stderr, "Recv from %s\n", sender);

    struct sockaddr_in *sin = (struct sockaddr_in *) addr;
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
   // inet_pton(AF_INET, sin->sin_addr,  sizeof(ip));
    port = htons (sin->sin_port);
    printf("port = %d \n",port);

    int sentBytes;
   // char sender[17] = { 0 };
    char replybuf[60];
   // memset(&replybuf,0, sizeof(replybuf));
    uv_ip4_name((const struct sockaddr_in*) addr, sender, 16);
    fprintf(stderr, "Recv from %s\n", sender);
    for(int i=0;i<nread;i++){fprintf(stderr,"%02X ",buf->base[i]);}
    fprintf(stderr,"\n");
    if((buf->base[0] & 0xFF) == 0xEF && (buf->base[1] & 0xFF) == 0xFE) {
	fprintf(stderr,"discovery packet detected\n");
	}

////////////// reply ///////////////

    uv_udp_send_t send_req;
 //   uv_buf_t discover_msg = make_discover_msg();

 	puts("create discovery reply buf");

	char b[60];
	for(int i=0;i<60;i++) {b[i] = 0;}
	b[0] = 0xEF;
	b[1] = 0xFE;
	b[2] = 0x02;
	const uv_buf_t a[]={{.base = b, .len=60}};

	for(int i=0;i<60;i++){fprintf(stderr,"%02X ",a[0].base[i]);}; 
	fprintf(stderr,"\n");

    struct sockaddr_in send_addr;
    uv_ip4_addr("255.255.255.255",67 , &send_addr);  

  //  send_addr = addr;  // copy address received to sending address
  //  uv_ip4_addr("168.192.1.90", 1024, &send_addr);
    puts("ready to send");
   // uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&sin, on_UDP_send);
  //  port = htons (servaddr->sin_port);
  //  printf("serv port = %d \n",port);

   servaddr.sin_port = htons(port);
   uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&servaddr, on_UDP_send);
 //   uv_udp_send(&send_req, &send_socket, a, 1, (const struct sockaddr *)&send_addr, on_UDP_send);
   puts("tried send");



//////////////////////
    free(buf->base);
    uv_udp_recv_stop(req);   // ???
}




int main() {
  puts("starting");
  loop = uv_default_loop();

    uv_udp_init(loop, &recv_socket);
    struct sockaddr_in recv_addr;
    uv_ip4_addr("0.0.0.0", 1024, &recv_addr);
    uv_udp_bind(&recv_socket, (const struct sockaddr *)&recv_addr, UV_UDP_REUSEADDR);
    uv_udp_recv_start(&recv_socket, alloc_buffer, on_UDP_read);

    uv_udp_init(loop, &send_socket);
    struct sockaddr_in broadcast_addr;
    uv_ip4_addr("0.0.0.0", 0, &broadcast_addr);
    uv_udp_bind(&send_socket, (const struct sockaddr *)&broadcast_addr, 0);
    uv_udp_set_broadcast(&send_socket, 1); puts("wait for UDP handshake\n");
  
  



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


  puts("start loop");
  return uv_run(loop, UV_RUN_DEFAULT);
}
