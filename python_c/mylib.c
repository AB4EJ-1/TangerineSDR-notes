

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <string.h>
#include <errno.h>

#include "mylib.h"

#define MAX_DEVICES 16


struct _DISCOVERED {
  union {
    int protocol;
    int device;
    int use_tcp;    // if set, use TCP rather than UDP to connect to radio
    char name[64];
    int software_version;
    int status;
    union {
      struct network {
        unsigned char mac_address[6];
        int address_length;
        struct sockaddr_in address;
        int interface_length;
        struct sockaddr_in interface_address;
        struct sockaddr_in interface_netmask;
        char interface_name[64];
      } network;
#ifdef LIMESDR
      struct soapy {
        SoapySDRKwargs *args;
      } soapy;
#endif
    } info;
  } info1;
  char infoblock[212];
 };
 

typedef struct _DISCOVERED DISCOVERED;


DISCOVERED discovered[MAX_DEVICES];



 
void test_empty(void)
{
     puts("Hello from C");
     printf("size of discovered array=%li\n",sizeof(discovered[0]));
}
 
float test_add(float x, float y)
{
     return x + y;
}
 
void test_passing_array(int *data, int len)
{
     printf("Data as received from Python\n");
     for(int i = 0; i < len; ++i) {
         printf("%d ", data[i]);
     }
     puts("");
 
     // Modifying the array
     for(int i = 0; i < len; ++i) {
         data[i] = -i;
     }
}

void test_passing_struct(char *datablock)
{
  strcpy(discovered[0].infoblock, "ABCDE");
  printf("put discovered into pointed area");
//  strcpy(datablock,"ABCD");  //works
  memcpy(datablock,discovered[0].infoblock,sizeof(discovered[0].infoblock));

}


