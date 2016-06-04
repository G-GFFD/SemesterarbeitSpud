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
#include "spud.h"


#define NETLINK_USER 31
#define MAX_PAYLOAD 102400
#define SERVER "127.0.0.1"
#define PORT 3333

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;


int main(int argc, char* argv[])
{
	// Part 1. Initiate connection with Kernel
	sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
	
	if(sock_fd<0)
	{
	printf("Opening Netlink socket failed!");
	return -1;
	}

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); // eigene Prozess ID

	bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;
	dest_addr.nl_groups = 0;

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	strcpy(NLMSG_DATA(nlh), "Hello"); // just that the Kernel knows the pid . . .

	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	sendmsg(sock_fd,&msg,0);

	// Userspace Programm bekommt jetzt die Nachrichten des Kernelspace . . .

	// Userspace Programm soll jetzt auch noch SPUD Packete übers Netzwerk empfangen
	
	struct sockaddr_in myself;

    	int s, i, slen=sizeof(struct sockaddr_in);
	void *buf = malloc(MAX_PAYLOAD);
 
	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
	printf("Failed to create socket!");
	}
 
	memset((char *) &myself, 0, sizeof(myself));
	myself.sin_family = AF_INET;
	myself.sin_port = htons(PORT-1); // htons(argv[1]);

	myself.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(s, &myself, sizeof(myself));

	int tcpdatalenght;
	int size;

	while(1)
	{
		//1. Teil SPUDS empfangen und hier injecten

		struct sockaddr_in* spudsource = malloc(sizeof(struct sockaddr_in));
		
		if(size = recvfrom(s, buf, MAX_PAYLOAD, MSG_DONTWAIT,spudsource, &slen) != -1)
		{   	
			// Extract SPUD from received UDP

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
			
			//was was wohin sendet nocheinmal anschauen
			inet_aton(SERVER, &spudsource->sin_addr);
			spudsource->sin_port = htons(3332);
			
			handlereceivedpacket(spud,spudsource);
						
			//setzen wir alles mal wieder auf null für die nächste runde
			memset(buf,0,MAX_PAYLOAD);
			
		}

		else
		{
			//No UDP Packet ready to be received
			free(spudsource);
		}

		//2. Teil TCP aus Kernelspace über SPUD wegsenden
	
		//Dieser Teil muss noch überarbeitet werden. 

		// Receive IPHDR First.. . 

		recvmsg(sock_fd, &msg, 0);
		struct iphdr *iph = malloc(sizeof(struct iphdr));
		memcpy(iph,NLMSG_DATA(nlh), sizeof(struct iphdr));
	
		// TCP Header Next . . .
		recvmsg(sock_fd, &msg, 0);
		struct tcphdr *tcph = malloc(sizeof(struct tcphdr));
		memcpy(tcph, NLMSG_DATA(nlh), sizeof(struct tcphdr));

		// Last receive tcp_data:
		recvmsg(sock_fd, &msg, 0);
		
		tcpdatalenght = (int)(iph->tot_len)-((int)(tcph->doff)+((int)iph->ihl))*4;
		char *data = malloc(tcpdatalenght);
		memcpy(data, NLMSG_DATA(nlh), tcpdatalenght);
		
		//setup struct soackaddr_in for external receiver of our spud packet
		struct sockaddr_in* receiver = malloc(sizeof(struct sockaddr_in));
		memset((char *) receiver, 0, sizeof (struct sockaddr_in));
		
		//send everything locally for now -> to be changed
		receiver->sin_family = AF_INET;
		inet_aton(SERVER, &receiver->sin_addr);
		receiver->sin_port= htons(PORT);

		//tcpverbindung des packets identifizieren
		struct tcptuple* tcpstream = malloc(sizeof(struct tcptuple));

		tcpstream->srcip = malloc(sizeof(char)*16);// size for ipv4 adress as char-string
		tcpstream->destip = malloc(sizeof(char)*16); //16, dami tplatz für 0 byte für termination?
		
		strcpy(tcpstream->srcip,inet_ntoa(*(struct in_addr*)&iph->saddr));
		strcpy(tcpstream->destip,inet_ntoa(*(struct in_addr*)&iph->daddr));

		tcpstream->srcport = ntohs(tcph->source);
		tcpstream->destport = ntohs(tcph->dest);

		if(tcph->syn == 1)
		{
			//Neuer TCP Stream entdeckt.  . . öffne neue SPUD Tube
			opennewtube(receiver, tcpstream); 
		}
		
		// Prüfen ob für diese TCP Sequenz / IP bereits eine Tube offen ist, ansonsten error

		uint8_t* temptube = findtcptuple(tcpstream);

		if(temptube != NULL)
		{
			printf("Existing Tube found, sending spud . . .\n");
			struct listelement* this = searchlist(temptube);
			if(this != NULL)
			{
				sendspud(this->fd,createspud(temptube,iph, tcph,data));
			}		
		}

		else
		{
			//No active tube for this packet found, drop.
		}		
	
		free(tcpstream->srcip);
		free(tcpstream->destip);
		free(tcpstream);
		free(tcph);
		free(iph);
		free(data);
		free(receiver);
	}	

	//Closing UDP Socket
	close(s);
	//Closing Socket to kernel
	close(sock_fd);
}
