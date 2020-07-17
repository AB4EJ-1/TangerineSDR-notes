#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include "de_signals.h"
//#define IP_FOUND "IP_FOUND"
//#define IP_FOUND_ACK "IP_FOUND_ACK"
#define PORT 1024

#define  DE_CONF_IN 50001  //fixed port on which to receive config request (CC)
#define  DE_CH_IN   50002   // fixed port on which to receive channel setup req. (CH)

#define UDPPORT 7100

void main(int argc, char** argv) {

 printf("arguments %s  %s\n",argv[1],argv[2]);
int i = 0;
 while (1==1)
  {
  printf("processing %i...\n",i);
  i++;
  sleep(5);
  }

 

}

