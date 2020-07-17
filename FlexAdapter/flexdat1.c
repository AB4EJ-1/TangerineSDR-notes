#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define UDPPORT_IN 7788
#define UDPPORT_OUT 7100

struct flexDataSample
  {
 // int16_t filler1;
  float I_val_int;
//  int16_t filler2;
  float Q_val_int;
  };

typedef struct flexDataBuf
 {
 char VITA_hdr1[2];  // rightmost 4 bits is a packet counter
 int16_t  VITA_packetsize;
 char stream_ID[4];
 char class_ID[8];
 uint32_t time_stamp;
 uint64_t fractional_seconds;
 struct flexDataSample flexDatSample[512];
 } FLEXBUF;

struct sockaddr_in flex_addr;
struct sockaddr_in output_addr;\

static  int fd;
static struct flexDataBuf iqbuffer;

void main () {

 int sock;  // input for flex data
 int sock1;
 int flexport = UDPPORT_IN;
 fd_set readfd;
 int count;
 uint32_t timeDelta = 0;
 

 sock = socket(AF_INET, SOCK_DGRAM, 0);
 if(sock < 0) {
  printf("sock error\n");
  return;
  }

 int addr_len = sizeof(struct sockaddr_in);
 memset((void*)&flex_addr, 0, addr_len);
 flex_addr.sin_family = AF_INET;
 flex_addr.sin_addr.s_addr = htons(INADDR_ANY);
 flex_addr.sin_port = htons(UDPPORT_IN);
 int ret = bind(sock, (struct sockaddr*)&flex_addr, addr_len);
 if (ret < 0){
  printf("bind error\n");
  return;
  }
 FD_ZERO(&readfd);
 FD_SET(sock, &readfd);
 printf("read from port %i\n",UDPPORT_IN);

 ret = 1;
 //while(1==1) {  // repeating loop
  if(ret > 0){
   if (FD_ISSET(sock, &readfd)){
  //  printf("try read\n");
    count = recvfrom(sock, &iqbuffer, sizeof(iqbuffer),0, (struct sockaddr*)&flex_addr, &addr_len);
    printf("bytes received = %i\n",count);
    printf("VITA header= %x %x\n",iqbuffer.VITA_hdr1[0],iqbuffer.VITA_hdr1[1]);
    printf("stream ID= %x%x%x%x\n", iqbuffer.stream_ID[0],iqbuffer.stream_ID[1], iqbuffer.stream_ID[2],iqbuffer.stream_ID[3]);

    printf("timestamp = %i \n",iqbuffer.time_stamp/16777216);
    for(int i=0;i<512;i++) {
     printf("%f %f \n",iqbuffer.flexDatSample[i].I_val_int,iqbuffer.flexDatSample[i].Q_val_int);
     }
    printf("\n");
  //  FILE * fptr;
  //  fptr = fopen("sampleIQ.dat","wb");
  //  fwrite(&iqbuffer,sizeof(iqbuffer),1,fptr);
    //close(fptr);
    }
 // }  // end of repeating loop
 }


 printf("end\n");

}
