
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

#include <pthread.h>


//#include "discovered.h"
//#include "discovery.h"

#define DEVICE_METIS 0
#define DEVICE_HERMES 1
#define DEVICE_GRIFFIN 2
#define DEVICE_ANGELIA 4
#define DEVICE_ORION 5
#define DEVICE_HERMES_LITE 6
// ANAN 7000DLE and 8000DLE uses 10 as the device type in old protocol
#define DEVICE_ORION2 10 
// Newer STEMlab hpsdr emulators use 100 instead of 1
#define DEVICE_STEMLAB 100

#ifdef USBOZY
#define DEVICE_OZY 7
#endif

#define NEW_DEVICE_ATLAS 0
#define NEW_DEVICE_HERMES 1
#define NEW_DEVICE_HERMES2 2
#define NEW_DEVICE_ANGELIA 3
#define NEW_DEVICE_ORION 4
#define NEW_DEVICE_ORION2 5
#define NEW_DEVICE_HERMES_LITE 6

#ifdef LIMESDR
#define LIMESDR_USB_DEVICE 0
#endif

#define STATE_AVAILABLE 2
#define STATE_SENDING 3

#define ORIGINAL_PROTOCOL 0
#define NEW_PROTOCOL 1
#ifdef LIMESDR
#define LIMESDR_PROTOCOL 2
#endif


#define MAX_DEVICES 16

struct _DISCOVERED {
    int protocol;
    int device;
    int use_tcp;    // use TCP rather than UDP to connect to radio
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
};

typedef struct _DISCOVERED DISCOVERED;

//extern int selected_device;
int selected_device;
int devices;

DISCOVERED discovered[MAX_DEVICES];
///////////// adapting to use pihpsdr discovery /////////////////////////////////////////

pthread_t discover_thread_id;  // thread ID



static char interface_name[64];
static struct sockaddr_in interface_addr={0};
static struct sockaddr_in interface_netmask={0};
static int interface_length;

#define DISCOVERY_PORT 1024
static int discovery_socket;
static struct sockaddr_in discovery_addr;

void new_discover(struct ifaddrs* iface);

//static GThread *discover_thread_id;
//gpointer new_discover_receive_thread(gpointer data);

void print_device(int i) {
    fprintf(stderr,"discovery: found protocol=%d device=%d software_version=%d status=%d address=%s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n", 
        discovered[i].protocol,
        discovered[i].device,
        discovered[i].software_version,
        discovered[i].status,
        inet_ntoa(discovered[i].info.network.address.sin_addr),
        discovered[i].info.network.mac_address[0],
        discovered[i].info.network.mac_address[1],
        discovered[i].info.network.mac_address[2],
        discovered[i].info.network.mac_address[3],
        discovered[i].info.network.mac_address[4],
        discovered[i].info.network.mac_address[5],
        discovered[i].info.network.interface_name);
}

void* new_discover_receive_thread(void* arg) {
//gpointer new_discover_receive_thread(gpointer data) {
    struct sockaddr_in addr;
    socklen_t len;
    unsigned char buffer[2048];
    int bytes_read;
    struct timeval tv;
    int i;

    tv.tv_sec = 2;
    tv.tv_usec = 0;

    setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

    len=sizeof(addr);
    while(1) {
        bytes_read=recvfrom(discovery_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&addr,&len);
        if(bytes_read<0) {
            fprintf(stderr,"new_discover: bytes read %d\n", bytes_read);
            perror("new_discover: recvfrom socket failed for discover_receive_thread");
            break;
        }
        fprintf(stderr,"new_discover: received %d bytes\n",bytes_read);
        if(bytes_read==1444) {
            if(devices>0) {
                break;
            }
        } else {
        if(buffer[0]==0 && buffer[1]==0 && buffer[2]==0 && buffer[3]==0) {
            int status = buffer[4] & 0xFF;
            if (status == 2 || status == 3) {
                if(devices<MAX_DEVICES) {
                    discovered[devices].protocol=NEW_PROTOCOL;
                    discovered[devices].device=buffer[11]&0xFF;
                    switch(discovered[devices].device) {
			case NEW_DEVICE_ATLAS:
                            strcpy(discovered[devices].name,"Atlas");
                            break;
			case NEW_DEVICE_HERMES:
                            strcpy(discovered[devices].name,"Hermes");
                            break;
			case NEW_DEVICE_HERMES2:
                            strcpy(discovered[devices].name,"Hermes2");
                            break;
			case NEW_DEVICE_ANGELIA:
                            strcpy(discovered[devices].name,"Angelia");
                            break;
			case NEW_DEVICE_ORION:
                            strcpy(discovered[devices].name,"Orion");
                            break;
			case NEW_DEVICE_ORION2:
                            strcpy(discovered[devices].name,"Orion2");
                            break;
			case NEW_DEVICE_HERMES_LITE:
                            strcpy(discovered[devices].name,"Hermes Lite");
                            break;
                        default:
                            strcpy(discovered[devices].name,"Unknown");
                            break;
                    }
                    discovered[devices].software_version=buffer[13]&0xFF;
                    for(i=0;i<6;i++) {
                        discovered[devices].info.network.mac_address[i]=buffer[i+5];
                    }
                    discovered[devices].status=status;
                    memcpy((void*)&discovered[devices].info.network.address,(void*)&addr,sizeof(addr));
                    discovered[devices].info.network.address_length=sizeof(addr);
                    memcpy((void*)&discovered[devices].info.network.interface_address,(void*)&interface_addr,sizeof(interface_addr));
                    memcpy((void*)&discovered[devices].info.network.interface_netmask,(void*)&interface_netmask,sizeof(interface_netmask));
                    discovered[devices].info.network.interface_length=sizeof(interface_addr);
                    strcpy(discovered[devices].info.network.interface_name,interface_name);
                    fprintf(stderr,"new_discover: found %d protocol=%d device=%d software_version=%d status=%d address=%s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n", 
                            devices,
                            discovered[devices].protocol,
                            discovered[devices].device,
                            discovered[devices].software_version,
                            discovered[devices].status,
                            inet_ntoa(discovered[devices].info.network.address.sin_addr),
                            discovered[devices].info.network.mac_address[0],
                            discovered[devices].info.network.mac_address[1],
                            discovered[devices].info.network.mac_address[2],
                            discovered[devices].info.network.mac_address[3],
                            discovered[devices].info.network.mac_address[4],
                            discovered[devices].info.network.mac_address[5],
                            discovered[devices].info.network.interface_name);
                    devices++;
                }
            }
        }
        }
    }
    fprintf(stderr,"new_discover: exiting new_discover_receive_thread\n");
    //g_thread_exit(NULL);
    pthread_exit(NULL);
    return NULL;
}

void new_discover(struct ifaddrs* iface) {
    int rc;
    struct sockaddr_in *sa;
    struct sockaddr_in *mask;
    char addr[16];
    char net_mask[16];
    int *retval_ptr;

    strcpy(interface_name,iface->ifa_name);
    fprintf(stderr,"new_discover: looking for HPSDR devices on %s\n",interface_name);

    // send a broadcast to locate metis boards on the network
    discovery_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(discovery_socket<0) {
        perror("new_discover: create socket failed for discovery_socket\n");
        exit(-1);
    }

    int optval = 1;
    setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    sa = (struct sockaddr_in *) iface->ifa_addr;
    mask = (struct sockaddr_in *) iface->ifa_netmask;

    interface_netmask.sin_addr.s_addr = mask->sin_addr.s_addr;

    // bind to this interface and the discovery port
    interface_addr.sin_family = AF_INET;
    interface_addr.sin_addr.s_addr = sa->sin_addr.s_addr;
    interface_addr.sin_port = htons(0);
    if(bind(discovery_socket,(struct sockaddr*)&interface_addr,sizeof(interface_addr))<0) {
        perror("new_discover: bind socket failed for discovery_socket\n");
        exit(-1);
    }

    strcpy(addr,inet_ntoa(sa->sin_addr));
    strcpy(net_mask,inet_ntoa(mask->sin_addr));

    fprintf(stderr,"new_discover: bound to %s %s %s\n",interface_name,addr,net_mask);

    // allow broadcast on the socket
    int on=1;
    rc=setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    if(rc != 0) {
        fprintf(stderr,"new_discover: cannot set SO_BROADCAST: rc=%d\n", rc);
        exit(-1);
    }

    // setup to address
    struct sockaddr_in to_addr={0};
    to_addr.sin_family=AF_INET;
    to_addr.sin_port=htons(DISCOVERY_PORT);
    to_addr.sin_addr.s_addr=htonl(INADDR_BROADCAST);

    // start a receive thread to collect discovery response packets
    //discover_thread_id = g_thread_new( "new discover receive", new_discover_receive_thread, NULL);
    pthread_create(&discover_thread_id, NULL, new_discover_receive_thread, NULL);
    if( ! discover_thread_id )
    {
        fprintf(stderr,"thread creation failed on new_discover_receive_thread\n");
        exit( -1 );
    }
    fprintf(stderr,"new_disovery: thread_id=%p\n",discover_thread_id);


    // send discovery packet
    unsigned char buffer[60];
    buffer[0]=0x00;
    buffer[1]=0x00;
    buffer[2]=0x00;
    buffer[3]=0x00;
    buffer[4]=0x02;
    int i;
    for(i=5;i<60;i++) {
        buffer[i]=0x00;
    }

    if(sendto(discovery_socket,buffer,60,0,(struct sockaddr*)&to_addr,sizeof(to_addr))<0) {
        perror("new_discover: sendto socket failed for discovery_socket\n");
        exit(-1);
    }

    // wait for receive thread to complete
  //  g_thread_join(discover_thread_id);
    pthread_join(discover_thread_id, (void**)&(retval_ptr));

    close(discovery_socket);

    fprintf(stderr,"new_discover: exiting discover for %s\n",iface->ifa_name);
}

void new_discovery() {
    struct ifaddrs *addrs,*ifa;
    puts("getifaddrs");
    getifaddrs(&addrs);
    ifa = addrs;
    while (ifa) {
        puts("iterate device list");
       // g_main_context_iteration(NULL, 0);
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            if((ifa->ifa_flags&IFF_UP)==IFF_UP
                && (ifa->ifa_flags&IFF_RUNNING)==IFF_RUNNING
                && (ifa->ifa_flags&IFF_LOOPBACK)!=IFF_LOOPBACK) {
                new_discover(ifa);
            }
        }
        ifa = ifa->ifa_next;
    }
    freeifaddrs(addrs);

    
    fprintf(stderr, "new_discovery found %d devices\n",devices);
    
    int i;
    for(i=0;i<devices;i++) {
        print_device(i);
    }
}

void main() {

  puts("start");
  new_discovery();
  }


