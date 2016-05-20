#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>

#include "spud.h"
 
#define SERVER "127.0.0.1"
#define BUFLEN 1024  //Wie lange für UDP Packet? Todo . . . 
#define PORT 3333
  
int main(void)
{
	struct sockaddr_in myself;
	struct sockaddr_in* spudsource;
	int s, i, slen=sizeof(struct sockaddr_in);
	void *buf = malloc(BUFLEN);
  
	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
        	printf("Failed to create socket!");
	}
 
	memset((char *) &myself, 0, sizeof(myself));
	myself.sin_family = AF_INET;
	myself.sin_port = htons(PORT);

	myself.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(s, &myself, sizeof(myself));

     
	if (inet_aton(SERVER , &spudsource->sin_addr) == 0) 
	{
        	printf("inet_aton() failed\n");
    	}

	printf("Ready to receive UDP Packets (SPUD)\n");
 
	while(1)
	{
		//printf("while\n");
		if (recvfrom(s, buf, BUFLEN, 0,spudsource, &slen) == -1)
	        {
			printf("Problem while receiving UDP Packet . . .");
	
        }
         
	
	struct spudpacket* spud = malloc(sizeof(struct spudpacket));
	spud = (struct spudpacket*) buf;
	
	handlereceivedpacket(spud,spudsource);
	
      }
 
    close(s);
    return 0;
}
