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
#include <sys/ioctl.h>
#include <net/if.h>
#include "tcphandling.h"
#include <sys/types.h>
#include <unistd.h>

#define SIZENLMSGBUFFER 102400
#define NETLINK_USER 31
#define MAX_PAYLOAD 1500
#define SERVER "127.0.0.1"
#define PORT 3333
#define HELLO "hello" //defines init message between this userspace programm and the kernel Module


int main()
{

	struct sockaddr_nl src_addr, dest_addr;
	struct nlmsghdr *nlh = NULL;
	struct iovec iov;
	int sock_fd;
	struct msghdr msg;

	memset(&msg,0, sizeof(struct msghdr));

	//Make sure Kernel Module is loaded:
	
	//load it in a binary buffer
	/*int fdm = fopen("kerneltcp.ko",rb);
	if(fdm < 0)
	{
		printf("problem opening file");
		fflush(stdout);
	}
	fseek(fdm, 0, SEEK_END);
	int len=ftell(fdm);
	void* buff = malloc(len);
	fseek(fdm, 0, SEEK_SET);
	fread(buff,len,1,fdm);
	fclose(fdm);

	if(init_module(buff,len,"") < 0)
	{
		printf("Failed to load required Kernel Module\n");
	}*/

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

	//char* message = malloc(strlen(HELLO));

	int length = SIZENLMSGBUFFER;
	
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

	sendmsg(sock_fd,&msg,0);

	// Userspace Programm bekommt jetzt die Nachrichten des Kernelspace . . .

	// Userspace Programm soll jetzt auch noch SPUD Packete übers Netzwerk empfangen
	
	struct sockaddr_in myself;

    	int s; //i;
	socklen_t slen=sizeof(struct sockaddr_in);
	void *buf = malloc(MAX_PAYLOAD);
 
	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
	printf("Failed to create socket!");
	}
 
	memset((char *) &myself, 0, sizeof(myself));
	myself.sin_family = AF_INET;
	myself.sin_port = htons(PORT-1); // htons(argv[1]);

	myself.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(s, (const struct sockaddr*) &myself, sizeof(myself));

	int size;
	uint8_t* temptube;
	struct tcptuple* tcpstream;

	while(1)
	{
		printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - \n");
		//1. Teil SPUDS empfangen und hier injecten

		struct sockaddr_in* spudsource = malloc(sizeof(struct sockaddr_in));
		
		if((size = recvfrom(s, buf, MAX_PAYLOAD, MSG_DONTWAIT,(struct sockaddr*) spudsource, &slen)) != -1)
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

			printf("\n");
			
		}

		else
		{
			//No UDP Packet ready to be received
			free(spudsource);
		}


		//2. Teil TCP aus Kernelspace über SPUD wegsenden
	
		// Extract IPHDR first, see if there are any options 

		// Set Flag to MSG_DONTWAIT . . .. aber dann läuft es viel zu schnell durch die schleife
	
		if((size = recvmsg(sock_fd, &msg, 0)) != -1)
		{
			void *message = malloc(SIZENLMSGBUFFER);
			memcpy(message,NLMSG_DATA(nlh),SIZENLMSGBUFFER);

			struct iphdr *iph = NULL;
			struct tcphdr *tcph = NULL;
			char *data = NULL;

			iph = extractiph(message);
			tcph = extracttcph(message,iph);
			data = extracttcpdata(message,iph,tcph);
			
			if(iph == NULL || tcph == NULL)
			{
				//problem, was nun?
				break;
			}
			
			free(message);
			//setup struct soackaddr_in for external receiver of our spud packet
			struct sockaddr_in* receiver = malloc(sizeof(struct sockaddr_in));
			memset((char *) receiver, 0, sizeof (struct sockaddr_in));
		
			//send everything locally for now -> to be changed
			receiver->sin_family = AF_INET;
			inet_aton(SERVER, &receiver->sin_addr);
			receiver->sin_port= htons(PORT);

			//tcpverbindung des packets identifizieren
			tcpstream = malloc(sizeof(struct tcptuple));

			tcpstream->srcip = malloc(sizeof(char)*16);// size for ipv4 adress as char-string
			tcpstream->destip = malloc(sizeof(char)*16); //16, dami tplatz für 0 byte für termination?
		
			strcpy(tcpstream->srcip,inet_ntoa(*(struct in_addr*)&iph->saddr));
			strcpy(tcpstream->destip,inet_ntoa(*(struct in_addr*)&iph->daddr));

			tcpstream->srcport = ntohs(tcph->source);
			tcpstream->destport = ntohs(tcph->dest);

			temptube = findtcptuple(tcpstream);
			if(tcph->syn == 1)
			{
				printf("TCP SYN Flag is set\n");
				printf("Total TCP Packet size (tcp options + tcp data) which shoud be < 1460 bytes is %i \n", (iph->tot_len)-((iph->ihl)*4+sizeof(struct tcphdr)));
				//check if TCP MSS Option is set
				
				//32bit binary to set tcp option mss to 1446 byte
				uint8_t mssoption[4] = {00000010, 00000100, 11111101,10100110};					//00000010, 00000100, 00000101,10100110
				
				if((tcph->doff)*4 > sizeof(struct tcphdr))
				{
					//There are options
					int numberofoptions = tcph->doff - sizeof(struct tcphdr)/4;
					
					//check if one of them sets mss
					int i, istrue = 0;

					for(i=0; i<numberofoptions; i++)
					{
						if(memcmp(&mssoption,tcph+(tcph->doff)+i*4,1) == 0)
						{
							//MSS Option found
							istrue = 1;
							
							//check if mss <= 1446, this is the maximum size of tcp options + data so that the injected ethernet packet at the receiver is max. 1500 bytes
							
							uint16_t currentmss = 0;
							memcpy(&currentmss,(void *)tcph+(tcph->doff)+i*4+2,2);

							if(currentmss <= (uint16_t) 1446)
							{
								//no problem then
								printf("MSS small already enough :)\n");
								break;
							}
							
							else
							{
								//copy smaller MTU in option
								memcpy((void *)tcph+(tcph->doff)+i*4+2,mssoption+2,2);
								updatetcpchecksum(tcph,iph,data);
								printf("current bigger TCP mss changed to 1446 bytes\n");
								//recaluclate IP Checksum
								updateipchecksum(iph);
							}
						}
					}

					if(istrue != 1)
					{
						//some options are present, but not the MSS
						
						int newsize = (tcph->doff)*4;						

						tcph = realloc(tcph,((tcph->doff)*4)+4);
						//Add MSS option
						memcpy(((void *)tcph)+((tcph->doff)*4),mssoption,4);

				 		//adjust doff
						tcph->doff = (tcph->doff)+1;//+= 1;
						//update checksum
						updatetcpchecksum(tcph, iph, data);
						printf("added additional option to limit TCP mss to 1446 bytes\n");
						iph->tot_len = iph->tot_len + 4;
						updateipchecksum(iph);
					}
					
				}

				else
				{
					//No options present
					tcph = realloc(tcph,sizeof(struct tcphdr)+4);
					//Add MSS option
					memcpy((void *)tcph+sizeof(struct tcphdr),mssoption,4);
				 	//adjust doff
					tcph->doff = tcph->doff+1;//+= 1;
					//update checksum
					updatetcpchecksum(tcph,iph,data);
					printf("added option to limit TCP mss to 1446 bytes\n");
					iph->tot_len = iph->tot_len + 4;
					updateipchecksum(iph);
					
				}

				if(temptube == NULL)
				{
					//Neuer TCP Stream entdeckt für den noch keine tube existiert
	
					//Debug print new TCP STream
					printf("\n \n Opening Tube for: \n \n");
					printf("Src IP: %s Src Port %i \n",tcpstream->srcip,tcpstream->srcport);	
					printf("Dest IP: %s Dest Port %i \n", tcpstream->destip, tcpstream->destport);		
					opennewtube(receiver, tcpstream); //opennewtube könnte eigentlich tubeid zurückgeben			
					temptube = findtcptuple(tcpstream);
			
					//printing tubeid
					if(temptube != NULL)
					{
						printf("Tubeid: ");
						int i;
						for(i=0; i<8; i++)
						{
							printf("%i ",temptube[i]);
						}
						printf("\n \n");
					}
			
					else
					{
						printf("Temptube ist NULL obwohl opentubes eigentlich eine angelegt haben sollte  . . .\n");
					}
				}

				else
				{
					printf("Syn Flag set, but Tube alreasy exists\n");
				}	
			}
		
			// Prüfen ob für diese TCP Sequenz / IP bereits eine Tube offen ist, ansonsten error

			if(temptube != NULL)
			{
				struct listelement* this = searchlist(temptube);
				if(this != NULL)
				{
					printf("Sending SPUD\n");
					sendspud(this->fd,createspud(temptube,iph, tcph,data));
				}
			}

			else
			{
				//No active tube for this packet found, drop.
				printf("No active Tube for this TCP Stream found, dropping packet . . .\n");
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

	//Closing UDP Socket
	close(s);
	//Closing Socket to kernel
	close(sock_fd);

	return 1;
}
