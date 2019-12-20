
#include<stdio.h>	
#include<stdlib.h>
#include <stdbool.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>

#define BUFFERLEN 65	// Maximum length of buffer
#define PORT 1024	// Port to watch

void crash(char *s)
{
	perror(s);
	exit(1);
}

// Routine to watch for discovery packets using old OpenHPSDR protocol
// Note that this is blocking. DEsimulator will only start accepting control
// commands after successful receipt of discovery packet.

void UDPhandshake(char **theIP)
{
	bool connected = 0;
	struct sockaddr_in si_DE, si_main;	
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
		char *theotherIP = malloc(16);
		strcpy(theotherIP, inet_ntoa(si_main.sin_addr));
		printf("Data: %s\n" , buf);		
		//now reply to the client with the same data
		if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_main, slen) == -1)
		{
			crash("ERROR - sendto() failed");
		}
  // here we pass the client's IP back to the caller via pointer
		*theIP = theotherIP;
		printf("theIP = %s \n", *theIP);
		if(connected) { close(s); return;}
	}

	close(s);
	return ;
}
