// Userspace handling of the filtered TCP Data & Sending packet over simple SPUD without CBOR

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/ip.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/tcp.h>
//#include <linux/in.h>
//#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

#include "spud.h"

#define NETLINK_USER 31
#define MAX_PAYLOAD 1024
#define SERVER "127.0.0.1"
#define PORT 3333

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;


int main()
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
	// z.B. Ack auf Öffnung neuer tubes

	struct sockaddr_in myself;

    	int s, i, slen=sizeof(struct sockaddr_in);
	void *buf = malloc(MAX_PAYLOAD);
 
	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
	printf("Failed to create socket!");
	}
 
	memset((char *) &myself, 0, sizeof(myself));
	myself.sin_family = AF_INET;
	myself.sin_port = htons(PORT);

	myself.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(s, &myself, sizeof(myself));

	/*if (inet_aton(SERVER , &spudsource.sin_addr) == 0) 
	{
        	printf("inet_aton() failed\n");
	}*/ //wieso sollte das nötig sein, darin sollte die src adresse des empfangenen packets stehen. . .
	int tcpdatalenght;
	while(1==1)
	{
	
		// Einmal vom Kernel Messages bekommen, einmal vom SPUD socket (z.B. öffnungs-ack)

		//1. Teil SPUDS empfangen und hier injecten

		struct sockaddr_in* spudsource = malloc(sizeof(struct sockaddr_in));

		// Check if an UDP Packet is ready to be received
		// set socket nonblocking first!!!
		//setnonblocking(s); FIX THIS
		//fcntl(s,O_NONBLOCK,0);
		//printf("Ready to receive UDP Packets (SPUD)\n");

		if(recvfrom(s, buf, MAX_PAYLOAD, MSG_DONTWAIT,spudsource, &slen) != -1)
		{   	
			// Received UDP Packet, first check if its a valid SPUD (magic number)
			struct spudpacket *spud = malloc(sizeof(struct spudpacket));
			memcpy(spud,buf, MAX_PAYLOAD);
			
			handlereceivedpacket(spud, spudsource);
			
			//setzen wir alles mal wieder auf null für die nächste runde
			memset(buf,0,MAX_PAYLOAD);
			
		}

		else
		{
			printf("No UDP Packet waiting to be received\n");
			free(spudsource);
		}

		//2. Teil TCP aus Kernelspace über SPUD wegsenden
		//jetzt wieder packet aus kernelspace empfangen (diese sind blocking . . .)
		//printf("packete aus kernelspace empfangen . . .");


		//To fix! 1. Malloc 2. länge? 3. unten free? 
		// Receive IPHDR First.. . 

		//Crasht manchmal bei diesem recvmsg . . . wieso unknown, ev. msg immer freigeben/ausnullen?
		recvmsg(sock_fd, &msg, 0);
		struct iphdr *iph = malloc(sizeof(struct iphdr));
		memcpy(iph,NLMSG_DATA(nlh), sizeof(struct iphdr));
	
		// TCP Header Next . . .
		recvmsg(sock_fd, &msg, 0);
		struct tcphdr *tcph = malloc(sizeof(struct tcphdr));
		memcpy(tcph, NLMSG_DATA(nlh), sizeof(struct tcphdr));


		// Last receive tcp_data:
		recvmsg(sock_fd, &msg, 0);
		
		//auf grösse tcp data lenght aus iphdr und tcphdr ausrechnen
		tcpdatalenght = (int)(iph->tot_len)-((int)(tcph->doff)+((int)iph->ihl))*4;
		char *data = malloc(tcpdatalenght);
		memcpy(data, NLMSG_DATA(nlh), tcpdatalenght);
		//data = (char *) NLMSG_DATA(nlh);
		
		//setup struct soackaddr_in for external receiver of our spud packet
		struct sockaddr_in* receiver = malloc(sizeof(struct sockaddr_in));
		memset((char *) receiver, 0, sizeof (struct sockaddr_in));
		receiver->sin_family = AF_INET;
		receiver->sin_port = htons(PORT);

		//Sendet packet an tcp zieladresse (z.B. ins internet) Vorerst besser alles lokal 127.0.0.1
		//inet_aton(inet_ntoa(*(struct in_addr*)&iph->saddr), &(*receiver).sin_addr); //check ob hier die richtige adresse
		//receiver->sin_addr = iph->saddr;	

		inet_aton("127.0.0.1", &receiver->sin_addr);

		if(tcph->syn == 1)
		{
			//Neuer TCP Stream entdeckt.  . . öffne neue SPUD Tube
			opennewtube(receiver); 
		}
		
		//else unnötig, da auch syn packet übertragen werden muss
		//else
		//{
		// Prüfen ob für diese TCP Sequenz / IP bereits eine Tube offen ist, ansonsten
		// error.
		uint8_t temptube = gettubeid(receiver);
		if(temptube != -1)
		{
			sendspud(receiver,createspud(temptube,iph, tcph,data));
		}

		else
		{
			printf("no active Tube for this Receiver, droping tcp data packet . . .\n");
		}		
		//}
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
