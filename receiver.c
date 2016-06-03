#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include <stddef.h>

#include "spud.h"
 
#define SERVER "127.0.0.1"
#define BUFLEN 15000 //Wie lange für UDP Packet? Todo . . . 
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
	
	printf("Size of Struct Spudheader: %i", sizeof(struct spudheader));
 	
	while(1)
	{
		printf("while\n");
		void *buf = malloc(BUFLEN); //für jedes Packet neuer buffer anlegen
		struct sockaddr_in* spudsource = malloc(sizeof(struct sockaddr_in)); //auch jeder receiver	
		int size;

		size = recvfrom(s, buf, BUFLEN, 0 ,spudsource, &slen);
		if(size != -1)
	        {
			/* ev. nötig zu testzwecken, aber eigetnlich einfach an source vom receiver zurück
			memset((char *) spudsource, 0, sizeof(struct sockaddr_in));
			spudsource->port = htons(PORT-1);     
			if (inet_aton(SERVER , &(spudsource->sin_addr)) == 0) 
			{
        			printf("inet_aton() failed\n");
    			}*/
			printf("Received packet of Size %i, extracting SPUD  . . . \n", size);
	
			struct spudpacket* spud = malloc(sizeof(struct spudpacket));
			struct spudheader* hdr = malloc(sizeof(struct spudheader));

			spud->hdr = hdr;

			memcpy(hdr,buf,sizeof(struct spudheader));
			
			spud->datalenght = size-sizeof(struct spudheader);
			if(spud->datalenght != 0)
			{
				spud->data = malloc(spud->datalenght);
				memcpy(spud->data,buf+sizeof(struct spudheader),spud->datalenght);
			}
			else
			{
				spud->data = NULL;
			}
			
			printf("spudmagic %i \n", *(spud->hdr));
			//handlereceivedpacket(spud,spudsource);
			
       		}
		
		else
		{
			printf("-1");
			free(buf);
			free(spudsource);
		}
       
	
	}
 
    close(s);
    return 0;
}
