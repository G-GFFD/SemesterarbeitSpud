// Userspace handling of the filtered TCP Data & Sending packet over simple SPUD without CBOR

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/ip.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "spud.h"
#include "tcphandling.h"

#define NETLINK_USER 31
#define MAX_PAYLOAD 1500
#define SERVER "10.2.115.157"
#define PORT 3333
#define HELLO "hello"

void *receivespud(void* argumnet)
{
	// Create UDP socket
	int size,s;
	struct sockaddr_in myself;
    
	socklen_t slen=sizeof(struct sockaddr_in);
	void *buf = malloc(MAX_PAYLOAD);
 
	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 )
	{
		printf("Failed to create socket!");
	}
 
	memset((char *) &myself, 0, sizeof(myself));
	myself.sin_family = AF_INET;
	myself.sin_port = htons(PORT-1);
	myself.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(s, (const struct sockaddr*) &myself, sizeof(myself));

	while(1)
	{
		//Receive UDP

		struct sockaddr_in* spudsource = malloc(sizeof(struct sockaddr_in));

		if((size = recvfrom(s, buf, MAX_PAYLOAD, 0,(struct sockaddr*) spudsource, &slen)) != -1)
		{   	
			printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - \n");
			printf("received udp\n");

			//Extract SPUD

			struct spudpacket* spud = malloc(sizeof(struct spudpacket));
			struct spudheader* hdr = malloc(sizeof(struct spudheader));

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
			
			//For now set fixed receiver, in final version Receiver is spudsource from recvfrom())
			inet_aton(SERVER, &spudsource->sin_addr);
			spudsource->sin_port = htons(3332); //Set standart SPUD Port where Receiver expects to receive SPUD packets
		
			HandleReceivedPacket(spud,spudsource);
						
			//Set buffers to zero
			memset(buf,0,MAX_PAYLOAD);

			printf("\n");
			
		}

		else
		{
			//some error happend while receiving UDP packet
		}

	}

	close(s);
}

void *tcptospud(void* argument)
{
	//Capture TCP from Kernel and send them over SPUD

	// First initiate connection with Kernel

	struct sockaddr_nl src_addr, dest_addr;
	struct nlmsghdr *nlh = NULL;
	struct iovec iov;
	int sock_fd;
	struct msghdr msg;

	memset(&msg,0, sizeof(struct msghdr));
	
	if( (sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_USER))<0)
	{
		printf("Opening Netlink socket failed! (Kernel Module not loaded?)");
		return -1;
	}

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); 

	bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;
	dest_addr.nl_groups = 0;

	int length = MAX_PAYLOAD;
	
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(length));
	memset(nlh, 0, NLMSG_SPACE(length));
	nlh->nlmsg_len = NLMSG_SPACE(length);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	strcpy(NLMSG_DATA(nlh), HELLO); // just that the Kernel knows the pid . . .

	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	int i= sendmsg(sock_fd,&msg,0);

	if(i < 0)
	{
		printf("Init message to kernel sent, error %s\n", strerror(errno));
	}

	while(1)
	{
		int size;
		uint8_t* temptube;
		struct tcptuple* tcpstream;	

		if((size = recvmsg(sock_fd, &msg, 0)) != -1)
		{
			printf(". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
			//printf("received new tcp of lenght %i from kernel\n", size);
			
			void *message = malloc(MAX_PAYLOAD);
			memcpy(message,NLMSG_DATA(nlh), MAX_PAYLOAD);

			struct iphdr *iph = NULL;
			struct tcphdr *tcph = NULL;
			char *data = NULL;

			iph = extractiph(message);
			tcph = extracttcph(message,iph);
			data = extracttcpdata(message,iph,tcph);
			
			updatetcpchecksum(tcph,iph,data);
			
			if(iph == NULL || tcph == NULL)
			{
				//problem, was nun?
				break;
			}
			
			free(message);

			//Set up sockaddr_in for external receiver
			struct sockaddr_in* receiver = malloc(sizeof(struct sockaddr_in));
			memset((char *) receiver, 0, sizeof (struct sockaddr_in));
		
			//Set receiver sockaddr_in (for now set to fixed receiver, in final version set to ip receiver on default SPUD port)
			receiver->sin_family = AF_INET;
			inet_aton(SERVER, &receiver->sin_addr);

			/*memcpy(&spudsource->sin_addr,iph->saddr);*/

			receiver->sin_port= htons(PORT-1);

			//Extract TCP tuple

			tcpstream = malloc(sizeof(struct tcptuple));

			tcpstream->srcip = malloc(sizeof(char)*16);// size for ipv4 adress as char-string
			tcpstream->destip = malloc(sizeof(char)*16); //16, dami tplatz für 0 byte für termination?
		
			strcpy(tcpstream->srcip,inet_ntoa(*(struct in_addr*)&iph->saddr));
			strcpy(tcpstream->destip,inet_ntoa(*(struct in_addr*)&iph->daddr));

			tcpstream->srcport = ntohs(tcph->source);
			tcpstream->destport = ntohs(tcph->dest);

			temptube = findtcptuple(tcpstream);

			if(temptube != NULL)
			{
				if(searchlist(temptube)->tcpfinsent == 1)
				{
					FinishTubeClosure(temptube, iph,tcph,data);
				}
			
				else
				{
					if(tcph->fin ==1)
					{
						InitiateTubeClosure(searchlist(temptube),iph,tcph,data);
					}

					else
					{
						SendSPUD( CreateSPUD(temptube, iph, tcph, data) );
					}
				}
			}
			
			else
			{
				//Debug print new TCP STream
				printf("\n \nOpening Tube for: \n \n");
				printf("Source IP: %s Port %i \n",tcpstream->srcip,tcpstream->srcport);	
				printf("Destination IP: %s Port %i \n", tcpstream->destip, tcpstream->destport);

				uint8_t* new = OpenNewTube(receiver,tcpstream);
				struct spudpacket *spud = CreateSPUD(new, iph, tcph, data);
				SendSPUD(spud);			
			}
			
				
	
			free(tcpstream->srcip);
			free(tcpstream->destip);
			free(tcpstream);
			free(tcph);
			free(iph);
			free(data);
			free(receiver);
		}
		
	}

	close(sock_fd);
}

void *status(void* argument)
{
	while(1)
	{
		// Check Tubes for Timeouts
		if(current != NULL) //at the beginning the list is empty
		{
			struct listelement* temp = current; // current must always point to the last element in the list
			int i = 0;

			while(temp->previous != NULL)
			{
				printf("\n\n\n\n\n ----------------------------------------------------- \n \n");
				i++;
				(temp->timeout)--;

				if(temp->timeout == 0)
				{
					//printf fremovingtube xxx
				
					close(temp->fd);

					if(temp->receiver != NULL)
					{
						free(temp->receiver);
					}

					else
					{
						printf("Info: Receiver to be freed was NULL . . . \n");
					}

					if(temp->tcp != NULL)
					{
						free(temp->tcp);
					}

					else
					{
						printf("Info: TCP tuple to be freed was NULL . . . \n");
					}

					
				
					temp = temp->previous;

					removetube(temp->next);
				}

				else
				{
					printf("Active Tube # %i - ", i);
					// tubeid, tcptuple etc. printen
					printf("Tubeid: ");
					
					int o;

					for(o=0; o<8; o++)
					{
						printf(" %i ", (int) temp->tubeid[o]);
					}
					
					printf("\n\nSource IP: %s Port %i \n",temp->tcp->srcip,temp->tcp->srcport);	
					printf("Destination IP: %s Port %i \n\n\n", temp->tcp->destip, temp->tcp->destport);		


					temp = temp->previous;
				}

			}
		
			// Display Status Infos about open Tubes
			printf("Number of Open Tubes: %i\n", i);

			printf("\n\n ----------------------------------------------------- \n\n");
		}

		else
		{
			printf("\n No active Tube yet \n");
		}
		
		sleep(10);
	}
}

int main(int argc, char* argv[])
{

	pthread_t ReceiveSPUD;
	pthread_t SendSPUD;
	pthread_t Status;

	//Check if Module Kerneltcp is loaded:
	//int file = fopen("/proc/modules", "r");, check if kerneltcp exists

	if(pthread_create(&ReceiveSPUD, NULL, receivespud, NULL))
	{
		printf("Error while creating thread to receive SPUDs\n");
	}

	if(pthread_create(&SendSPUD, NULL, tcptospud, NULL))
	{
		printf("Error while creating thread to send SPUDs\n");
	}

	if(pthread_create(&Status, NULL, status, NULL))
	{
		printf("Error while creating thread Status\n");
	}

	
	while(pthread_join(&ReceiveSPUD,NULL) != 0)
	{	

	}

	return 1;
}
