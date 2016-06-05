#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include <stddef.h>

#include "spud.h"
 
#define SERVER "127.0.0.1"
#define BUFLEN 16000 //Wie lange für UDP Packet? Todo . . . 
#define PORT 3333
  
int main(void)
{
	struct sockaddr_in myself;
	int s, slen=sizeof(struct sockaddr_in);
	
  	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
        	printf("Failed to create socket!");
	}
 
	memset((char *) &myself, 0, sizeof(myself));
	myself.sin_family = AF_INET;
	myself.sin_port = htons(PORT+1);
	myself.sin_addr.s_addr = htonl(INADDR_ANY);
	
	bind(s, &myself, sizeof(myself));

	printf("Ready to receive UDP Packets (SPUD) on Port %i\n", ntohs(myself.sin_port));
	
	int size;
	void *buf;
	struct sockaddr_in* spudsource;

	while(1)
	{
		printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - \n");
		buf = malloc(BUFLEN); //für jedes Packet neuer buffer anlegen
		spudsource = malloc(sizeof(struct sockaddr_in)); //auch jeder receiver	
		size = 0;

		size = recvfrom(s, buf, BUFLEN, 0 ,spudsource, &slen);
		if(size != -1)
	        {
			// Received UDP, extracting SPUD
			struct spudpacket* spud = malloc(sizeof(struct spudpacket));
			struct spudheader* hdr = malloc(sizeof(struct spudheader));
			
			printf("totalsize of received spud is: %i",size);
			spud->hdr = hdr;

			memcpy(hdr,buf,sizeof(struct spudheader));
			
			spud->datalenght = size-sizeof(struct spudheader);
			if(spud->datalenght > 0)
			{
				spud->data = malloc(spud->datalenght);
				memcpy(spud->data,buf+sizeof(struct spudheader),spud->datalenght);
			}
			else
			{
				spud->data = NULL;
			}
			
			//memset((char *) spudsource, 0, sizeof(struct sockaddr_in));
			inet_aton(SERVER, &spudsource->sin_addr);
			spudsource->sin_port = htons(3332);
	
			handlereceivedpacket(spud,spudsource);
			
       		}
		
		else
		{
			printf("Problem while receiving UDP Packet . . . continue");
			free(buf);
			free(spudsource);
		}
       
		printf("\n");
	
	}
 
    close(s);
    return 0;
}
