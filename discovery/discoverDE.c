
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
#include <fcntl.h>
#include <unistd.h>

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
pthread_t discover_thread_id_o; 

#define IPADDR_LEN 20
static char ipaddr_tcp_buf[IPADDR_LEN] = "10.10.10.10";
char *ipaddr_tcp = &ipaddr_tcp_buf[0];



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

// the following prints a list of devices found
void print_device(int i) {
    fprintf(stderr,"discovery list: found protocol=%d %s device=%d software_version=%d status=%d address=%s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n", 
        discovered[i].protocol,
        discovered[i].name,
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

/////////////////////// new discovery ////////////////////////

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
			for (int i; i<bytes_read;i++) {printf("%02X",buffer[i]); }; printf("\n");
            perror("new_discover: recvfrom socket failed for discover_receive_thread");
            break;
        }
        fprintf(stderr,"new_discover: received %d bytes\n",bytes_read);
        for (int i; i<bytes_read;i++) {printf("%02X",buffer[i]); }; printf("\n");
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
                    fprintf(stderr,"new_discover: found %d  %s protocol=%d device=%d software_version=%d status=%d address=%s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n", 
                            devices,
                            discovered[devices].name,
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

/////////////////////// old discovery /////////////////////////

static void *discover_receive_thread_o(void* arg) {
//static gpointer discover_receive_thread(gpointer data) {
    struct sockaddr_in addr;
    socklen_t len;
    unsigned char buffer[2048];
    int bytes_read;
    struct timeval tv;
    int i;

    fprintf(stderr,"discover_receive_thread_o\n");

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

    len=sizeof(addr);
    while(1) {
        bytes_read=recvfrom(discovery_socket,buffer,sizeof(buffer),1032,(struct sockaddr*)&addr,&len);
        fprintf(stderr,"Data received, bytes_read = %d \n",bytes_read);
        if(bytes_read<0) {
            fprintf(stderr,"discovery: bytes read %d\n", bytes_read);
            perror("discovery: recvfrom socket failed for discover_receive_thread_o");
            break;
        }
        if (bytes_read == 0) break;
        fprintf(stderr,"Old Protocol discovered: received %d bytes\n",bytes_read);
        for (int i; i<bytes_read;i++) {printf("%02X",buffer[i]); }; printf("\n");

        if ((buffer[0] & 0xFF) == 0xEF && (buffer[1] & 0xFF) == 0xFE) {
            int status = buffer[2] & 0xFF;
            if (status == 2 || status == 3) {
                if(devices<MAX_DEVICES) {
                    discovered[devices].protocol=ORIGINAL_PROTOCOL;
                    discovered[devices].device=buffer[10]&0xFF;
                    switch(discovered[devices].device) {
                        case DEVICE_METIS:
                            strcpy(discovered[devices].name,"Metis");
                            break;
                        case DEVICE_HERMES:
                            strcpy(discovered[devices].name,"Hermes");
                            break;
                        case DEVICE_GRIFFIN:
                            strcpy(discovered[devices].name,"Griffin");
                            break;
                        case DEVICE_ANGELIA:
                            strcpy(discovered[devices].name,"Angelia");
                            break;
                        case DEVICE_ORION:
                            strcpy(discovered[devices].name,"Orion");
                            break;
                        case DEVICE_HERMES_LITE:
							#ifdef RADIOBERRY
								strcpy(discovered[devices].name,"Radioberry");
							#else
								strcpy(discovered[devices].name,"Hermes Lite");		
							#endif

                            break;
                        case DEVICE_ORION2:
                            strcpy(discovered[devices].name,"Orion 2");
                            break;
			case DEVICE_STEMLAB:
			    // This is in principle the same as HERMES so pretend a HERMES
			    discovered[devices].device = DEVICE_HERMES;
                            strcpy(discovered[devices].name,"STEMlab");
                            break;
                        default:
                            strcpy(discovered[devices].name,"Unknown");
                            break;
                    }
                    discovered[devices].software_version=buffer[9]&0xFF;
                    for(i=0;i<6;i++) {
                        discovered[devices].info.network.mac_address[i]=buffer[i+3];
                    }
                    discovered[devices].status=status;
                    memcpy((void*)&discovered[devices].info.network.address,(void*)&addr,sizeof(addr));
                    discovered[devices].info.network.address_length=sizeof(addr);
                    memcpy((void*)&discovered[devices].info.network.interface_address,(void*)&interface_addr,sizeof(interface_addr));
                    memcpy((void*)&discovered[devices].info.network.interface_netmask,(void*)&interface_netmask,sizeof(interface_netmask));
                    discovered[devices].info.network.interface_length=sizeof(interface_addr);
                    strcpy(discovered[devices].info.network.interface_name,interface_name);
		    discovered[devices].use_tcp=0;
		    fprintf(stderr,"discovery: found radio %s device=%d software_version=%d status=%d address=%s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n",
                            discovered[devices].name,
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
    fprintf(stderr,"discovery: exiting discover_receive_thread_o\n");
    //g_thread_exit(NULL);
    pthread_exit(NULL);
    return NULL;
}


static void discover(struct ifaddrs* iface) {
    int rc;
    struct sockaddr_in *sa;
    struct sockaddr_in *mask;
    struct sockaddr_in to_addr={0};
    int flags;
    struct timeval tv;
    int optval;
    socklen_t optlen;
    fd_set fds;
    unsigned char buffer[1032];
    int i, len;
    printf("Starting standard discover process\n");
    int *retval_ptr;

    if (iface == NULL) {
	//
	// This indicates that we want to connect to an SDR which
	// cannot be reached by (UDP) broadcast packets, but that
	// we know its fixed IP address
	// Therefore we try to send a METIS detection packet via TCP 
	// to a "fixed" ip address.
	//
        fprintf(stderr,"Trying to detect at TCP addr %s\n", ipaddr_tcp);
	memset(&to_addr, 0, sizeof(to_addr));
	to_addr.sin_family = AF_INET;
	if (inet_aton(ipaddr_tcp, &to_addr.sin_addr) == 0) {
	    fprintf(stderr,"discover: TCP addr %s is invalid!\n",ipaddr_tcp);
	    return;
	}
	to_addr.sin_port=htons(DISCOVERY_PORT);

        discovery_socket=socket(AF_INET, SOCK_STREAM, 0);
        if(discovery_socket<0) {
            perror("discover: create socket failed for TCP discovery_socket\n");
            return;
	}
	//
	// Here I tried a bullet-proof approach to connect() such that the program
        // does not "hang" under any circumstances.
	// - First, one makes the socket non-blocking. Then, the connect() will
        //   immediately return with error EINPROGRESS.
	// - Then, one uses select() to look for *writeability* and check
	//   the socket error if everything went right. Since one calls select()
        //   with a time-out, one either succeed within this time or gives up.
        // - Do not forget to make the socket blocking again.
	//
        // Step 1. Make socket non-blocking and connect()
	flags=fcntl(discovery_socket, F_GETFL, 0);
	fcntl(discovery_socket, F_SETFL, flags | O_NONBLOCK);
	rc=connect(discovery_socket, (const struct sockaddr *)&to_addr, sizeof(to_addr));
        if ((errno != EINPROGRESS) && (rc < 0)) {
            perror("discover: connect() failed for TCP discovery_socket:");
	    // originally the following was a close(--) but not compiling
        close(discovery_socket);
	    return;
	}
	// Step 2. Use select to wait for the connection
        tv.tv_sec=3;
	tv.tv_usec=0;
	FD_ZERO(&fds);
	FD_SET(discovery_socket, &fds);
	rc=select(discovery_socket+1, NULL, &fds, NULL, &tv);
        if (rc < 0) {
            perror("discover: select() failed on TCP discovery_socket:");
// originally was close (--) but not compiling
	    close(discovery_socket);
	    return;
        }
	// If no connection occured, return
	if (rc == 0) {
	    // select timed out
	    fprintf(stderr,"discover: select() timed out on TCP discovery socket\n");
	    close(discovery_socket);
	    return;
	}
	// Step 3. select() succeeded. Check success of connect()
	optlen=sizeof(int);
	rc=getsockopt(discovery_socket, SOL_SOCKET, SO_ERROR, &optval, &optlen);
	if (rc < 0) {
	    // this should very rarely happen
            perror("discover: getsockopt() failed on TCP discovery_socket:");
	    close(discovery_socket);
	    return;
	}
	if (optval != 0) {
	    // connect did not succeed
	    fprintf(stderr,"discover: connect() on TCP socket did not succeed\n");
	    close(discovery_socket);
	    return;
	}
	// Step 4. reset the socket to normal (blocking) mode
	fcntl(discovery_socket, F_SETFL, flags &  ~O_NONBLOCK);
    } else {

        strcpy(interface_name,iface->ifa_name);
        fprintf(stderr,"discover: looking for HPSDR devices on %s\n", interface_name);

        // send a broadcast to locate hpsdr boards on the network
		fprintf(stderr,"Prepare to Broadcast discovery packet\n");
        discovery_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(discovery_socket<0) {
            perror("discover: create socket failed for discovery_socket:");
            exit(-1);
        }

        sa = (struct sockaddr_in *) iface->ifa_addr;
        mask = (struct sockaddr_in *) iface->ifa_netmask;
        interface_netmask.sin_addr.s_addr = mask->sin_addr.s_addr;

        // bind to this interface and the discovery port
        interface_addr.sin_family = AF_INET;
        interface_addr.sin_addr.s_addr = sa->sin_addr.s_addr;
        //interface_addr.sin_port = htons(DISCOVERY_PORT*2);
        interface_addr.sin_port = htons(0); // system assigned port
        if(bind(discovery_socket,(struct sockaddr*)&interface_addr,sizeof(interface_addr))<0) {
            perror("discover: bind socket failed for discovery_socket:");
            exit(-1);
        }

        fprintf(stderr,"discover: bound to %s\n",interface_name);

        // allow broadcast on the socket
        int on=1;
        rc=setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
        if(rc != 0) {
            fprintf(stderr,"discover: cannot set SO_BROADCAST: rc=%d\n", rc);
            exit(-1);
        }

        // setup to address
        to_addr.sin_family=AF_INET;
        to_addr.sin_port=htons(DISCOVERY_PORT);
        to_addr.sin_addr.s_addr=htonl(INADDR_BROADCAST);
    }
    optval = 1;
    setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    rc=devices;
    // start a receive thread to collect discovery response packets
    //discover_thread_id = g_thread_new( "old discover receive", discover_receive_thread, NULL);
    printf("starting old discovery thread\n");

 //   pthread_create(&discover_thread_id_o, NULL, new_discover_receive_thread, NULL);
    pthread_create(&discover_thread_id_o, NULL, discover_receive_thread_o, NULL);
    if( ! discover_thread_id_o )
    {
        fprintf(stderr,"pthread_new failed on new_discover_receive_thread\n");
        exit( -1 );
    }



    // send discovery packet
    // If this is a TCP connection, send a "long" packet
    len=63;
    if (iface == NULL) len=1032;
    buffer[0]=0xEF;
    buffer[1]=0xFE;
    buffer[2]=0x02;
    for(i=3;i<len;i++) {
        buffer[i]=0x00;
    }

    if(sendto(discovery_socket,buffer,len,0,(struct sockaddr*)&to_addr,sizeof(to_addr))<0) {
        perror("discover: sendto socket failed for discovery_socket:");
        exit(-1);
    }

    // wait for receive thread to complete
    //g_thread_join(discover_thread_id);
    pthread_join(discover_thread_id_o, (void**)&(retval_ptr));

    close(discovery_socket);

    if (iface == NULL) {
      fprintf(stderr,"discover: exiting TCP discover for %s\n",ipaddr_tcp);
      if (devices == rc+1) {
	//
	// We have exactly found one TCP device
	// and have to patch the TCP addr into the device field
	// and set the "use TCP" flag.
	//
        memcpy((void*)&discovered[rc].info.network.address,(void*)&to_addr,sizeof(to_addr));
        discovered[rc].info.network.address_length=sizeof(to_addr);
        memcpy((void*)&discovered[rc].info.network.interface_address,(void*)&to_addr,sizeof(to_addr));
        memcpy((void*)&discovered[rc].info.network.interface_netmask,(void*)&to_addr,sizeof(to_addr));
        discovered[rc].info.network.interface_length=sizeof(to_addr);
        strcpy(discovered[rc].info.network.interface_name,"TCP");
	discovered[rc].use_tcp=1;
      }
    } else {
      fprintf(stderr,"discover: exiting discover for %s\n",iface->ifa_name);
    }

}



void old_discovery() {
    struct ifaddrs *addrs,*ifa;

    fprintf(stderr,"old_discovery starting\n");
    getifaddrs(&addrs);
    ifa = addrs;
    while (ifa) {
      //  g_main_context_iteration(NULL, 0);
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
			#ifdef RADIOBERRY
			if((ifa->ifa_flags&IFF_UP)==IFF_UP
                && (ifa->ifa_flags&IFF_RUNNING)==IFF_RUNNING) {
				discover(ifa);
			}
			#else
            if((ifa->ifa_flags&IFF_UP)==IFF_UP
                && (ifa->ifa_flags&IFF_RUNNING)==IFF_RUNNING
                && (ifa->ifa_flags&IFF_LOOPBACK)!=IFF_LOOPBACK) {
                discover(ifa);
            }
			#endif 
        }
        ifa = ifa->ifa_next;
    }
    freeifaddrs(addrs);

    // Do one additional "discover" for a fixed TCP address
    //discover(NULL);

    fprintf(stderr, "discovery found %d devices\n",devices);

    int i;
    for(i=0;i<devices;i++) {
                    fprintf(stderr,"discovery: found device=%d software_version=%d status=%d address=%s (%02X:%02X:%02X:%02X:%02X:%02X) on %s\n",
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

}

//////////////////////////////////////////////////////////////

void main() {

  puts("LOOKING for old protocol **********************");
  old_discovery();
 // puts("LOOKING for new protocol **********************");
 // new_discovery();
  }


