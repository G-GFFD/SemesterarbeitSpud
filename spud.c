// Implementation of Simple SPUD without CBOR
#include <stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<linux/ip.h>
#include<linux/tcp.h>
#include<netinet/in.h>
#include "injectcp.h"
#include "spud.h"
#include <errno.h>
#include <time.h>

#define SERVER "10.2.119.166"

uint8_t magic[4] = {11011000,00000000,0000000,11011000};

struct spudheader* mallocspudheader()
{
	//This Function allocates space for the SPUD Header and initializes it with standart Values

	struct spudheader* hdr = malloc(sizeof(struct spudheader));
	
	memcpy(hdr->magic,magic,4);

	hdr->cmdflags = 0;
	
	//just to fix a crash caused by uninitialized values in mallocspudheader()
	int i;
	for(i=0; i<8; i++)
	{
		hdr->tubeid[i] = 0;
	}

	return hdr;
}

int idgenerator(uint8_t* tubeid)
{
	//This Function generates a random 64bit Tubeid and sets it into the spud header

	srand(time(NULL));
	int i;
	uint8_t temp;

	for(i=0;i<8; i++)
	{
		temp = (uint8_t) rand();
		memcpy(tubeid+sizeof(uint8_t)*i,&temp, sizeof(uint8_t));
	}
	
}

int opennewtube(struct sockaddr_in* receiver, struct tcptuple* tcp)
{
	//This Function opens a new Tube to a Receiver, adds it to the local list and sends the open spud packet

		//Open new Tube
		struct spudheader* newspudheader = mallocspudheader();
	
		//just to make sure new tube id is not already taken
		do{
		idgenerator(newspudheader->tubeid);
		}while(searchlist(newspudheader->tubeid) != NULL);
		//wihtout do-while I got multiple tubes with the same id if they were created immediatley after another, strange . . ..

		newspudheader->cmdflags = 0;
		newspudheader->cmdflags |= 1<<6; //set to 0b1000000;

		struct spudpacket* newspud = malloc(sizeof(struct spudpacket));
		newspud->hdr = newspudheader;
		newspud->datalenght = 0;
		newspud->data = NULL;

		//Create Status in List of Open Tubes
		struct listelement *newentry = malloc(sizeof(struct listelement));
		memcpy(newentry->tubeid,newspudheader->tubeid,8*sizeof(uint8_t));

		//copy tcptuple to store in list, as original tuple will be free in sender.c (due to current implementation)
		struct tcptuple* tubetcp = malloc(sizeof(struct tcptuple));
		memcpy(tubetcp,tcp,sizeof(struct tcptuple));

		//Same problem with the source/dest ip pointer as they'll be freed
		//Ip's nur pointer im tcptouple -> auch noch kopieren
		tubetcp->srcip = malloc(sizeof(char)*16);
		tubetcp->destip = malloc(sizeof(char)*16);

		strncpy(tubetcp->srcip, tcp->srcip, 16);
		strncpy(tubetcp->destip, tcp->destip, 16);

		newentry->tcp = tubetcp;
		newentry->receiver = receiver;
		newentry->status = 1;
		addtube(newentry);

		struct sockaddr_in localsource;
		int fd;

		//ganzer folgender abschnitt nochmals überdenken
		memset(&localsource,0, sizeof(struct sockaddr_in));
		inet_aton(SERVER, &receiver->sin_addr);
		receiver->sin_port = htons(3332);
		localsource.sin_port = htons(tubetcp->srcport-1);

		if ( (fd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		{
			printf("Failed to create socket!");
		}
		bind(fd, (struct sockaddr*) &localsource, sizeof (struct sockaddr_in));
		if(connect(fd, (struct sockaddr*) receiver, sizeof (struct sockaddr_in)) == -1)
		{
			printf("Error while connecting socket!");
		}

		newentry->fd=fd;

		sendspud(fd,newspud);	
	
}

struct spudpacket* createspud(uint8_t* tubeid, struct iphdr *iph, struct tcphdr *tcph, void* tcpdata)
{
	//This Function creates and returns a spudpacket out of a tubeid, iphdr, tcphdr and tcpdata

	struct spudpacket* spud = malloc(sizeof(spudpacket));
	struct spudheader* header = mallocspudheader();
	
	memcpy(header->tubeid, tubeid, 8*sizeof(uint8_t));
	header->cmdflags = 0; //header->cmdflags & 0b00000000;

	spud->hdr = header;

	int tcpdatalenght = (int)ntohs((iph->tot_len))-((int)(tcph->doff)+((int)iph->ihl))*4;

	int iphdrlen = ((int)(iph->ihl)*4); // sizeof(struct iphdr) doesn't contain the options appended at the end
	int tcphdrlen = ((int)(tcph->doff)*4);
		
	spud->datalenght = tcpdatalenght + iphdrlen + tcphdrlen;

	//check . . .
	if(ntohs(iph->tot_len) != spud->datalenght)
	{
		//This would indicate some data have gone missing.
		printf("ntohs(iph->tot_len) != spud->datalenght in spud.c ln 143\n");	
	}

	//This if due to crashes because of ridiculusly high datalenght which caused a crash (happend rarely)
	if(spud->datalenght < 20000)
	{
		spud->data = malloc(spud->datalenght);

		//first memcpy iphdr
		memcpy(spud->data, iph,iphdrlen);

		//second memcpy tcphdr
		memcpy(spud->data+iphdrlen, tcph, tcphdrlen);

		//finally memcpy tcpdata
		memcpy(spud->data+iphdrlen+tcphdrlen, tcpdata, tcpdatalenght);
	}

	else
	{
		printf("Fatal Error, TCP Datalenght too large: %i",spud->datalenght);
		//For now just returning an empty spud . . .
		//besser handeln, crasht manchmal weil spud->data kein pointer enthält bei free() . . .
	}

	return spud;
	
}


int sendspud(int fd, struct spudpacket* spud)
{
	//This Function sends the SPUD Packet to the fd

	//find out size of spudpacket & tcpdata . . .
	int size = sizeof(struct spudheader) + spud->datalenght;

	// allocate memory buf of this size . . .
	void *buf = malloc(size);
	
	//memcpy spudheader an den anfang kopieren
	memcpy(buf, spud->hdr, sizeof(struct spudheader));

	if(spud->datalenght > 0)
	{	
		//Falls daten, diese dahinter kopieren
		memcpy(buf+sizeof(struct spudheader), spud->data, spud->datalenght);
		free(spud->data);
	}

	printf("fd is %i, size is %i and buf starts at 0x%x \n",fd,size,buf);
	int temp = send(fd, buf, size, 0);

	printf("\nsent %i bytes \n",temp);
	if(temp == -1)
	{
		printf("Error: %s\n", strerror());
	}

	else
	{
		printf("\nSuccess sent %i bytes \n",temp);
	}
	
	//Socket schliessen bei closetube();
		
	free(buf);
	free(spud->hdr);
	free(spud);

	return 1;
	
}

int handlereceivedpacket(struct spudpacket* spud, struct sockaddr_in* receiver)
{
	//This Function handles received spud packets according to its cmdflags
	
	if(memcmp(magic,spud->hdr->magic,4)==0)
	{
		// ok valid spud packet
		
		//check if bits in cmdflags are set 01xxxx
		if((((spud->hdr->cmdflags & (1 << 6)) >> 6) &1) && (!((spud->hdr->cmdflags & (1 << 7)) >> 7) &1))
		{
			//received spud wants to open a new tube
			printf("Received SPUD that wants to open a new tube\n");
			//create socket for this receiver, bind
			
			acknowledgenewtube(spud, receiver);
		}
		
		//check if bits in cmdflags are set 10xxxxx
		else if((((spud->hdr->cmdflags & (1 << 7)) >> 7) &1) && (!((spud->hdr->cmdflags & (1 << 6)) >> 6) &1))
		{
			//received spud wants to close a tube
			printf("Received SPUD that wants to close a tube\n");
			closetube(spud->hdr->tubeid);
		}

		//check if bits in cmdflags are set 11xxxxx
		else if((((spud->hdr->cmdflags & (1 << 6)) >> 6) &1) && (((spud->hdr->cmdflags & (1 << 7)) >> 7) &1))
		{
			printf("Received SPUD that acknowledges a tube\n");
			//tube status auf 2, "running" updaten
			struct listelement* temp;
			temp = searchlist(spud->hdr->tubeid);
			if(temp != NULL)
			{
				temp->status = 2;
			}

		}
		
		//check if bits in cmdflags are set 00xxxx
		else if((!((spud->hdr->cmdflags & (1 << 6)) >> 6) &1) && (!((spud->hdr->cmdflags & (1 << 7)) >> 7) &1))
		{
			printf("Data Flag 00 detected\n\n");

			struct iphdr *iph = NULL;
			struct tcphdr *tcph = NULL;
			char *data = NULL;

			iph = extractiph(spud->data);
			tcph = extracttcph(spud->data,iph);
			data = extracttcpdata(spud->data,iph,tcph);

			updatetcpchecksum(tcph,iph,data);

			printf("\n\n Just extracted from data SPUD: \n");
			updatetcpchecksum(tcph,iph,data);

			printf("Total lenght of included IP Packet: %i bytes\n", ntohs(iph->tot_len));
			printf("Sizeof IPHDR with Options: %i bytes\n",4*(iph->ihl));
			printf("Sizeof TCPHDR with Options: %i bytes\n",4*(tcph->doff));
			printf("Sizeof TCP Data: %i bytes\n\n", ntohs(iph->tot_len)-((iph->ihl)+(tcph->doff))*4);

			if(iph != NULL && tcph != NULL)
			{
				injecttcp(iph, tcph, data);
				//printf("Iphdr, tcphdr, tcpdata extracted from spud, injection function called. . .\n");
			}

			else
			{
				printf("Extraction unsuccessfull, can't inject tcp\n");
			}

			free(spud->data);
			free(spud);
			free(receiver);
		}

		//wenn es korrekt läuft mit der überprüfung der einzelnen bits, dann unnötig
		else
		{
			printf("CORRECT MAGIC SPUD NUMBER BUT UNKNOWN FLAGS");
		}
		return 1;
	}

	else
	{
		printf("Hmmm received UDP Packet without Magic SPUD Number\n");

		free(spud);
		free(receiver);
		return 0; 
	}
}


int acknowledgenewtube(struct spudpacket* spud, struct sockaddr_in* receiver)
{		
	//This Function generates a SPUD Packet to acknowledge the request to open a tube

	//Add to list of open tubes, create element, create receiver and connect

	struct sockaddr_in localsource;
	int fd;
	memset(&localsource,0, sizeof(struct sockaddr_in));
	inet_aton(SERVER, &receiver->sin_addr);
	receiver->sin_port = htons(3332);
	//localsource.sin_port = htons(tubetcp->srcport-1); // fragen ob das schlau ist . . .

	if ( (fd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		printf("Failed to create socket!");
	}
	//bind(fd, (struct sockaddr*) &localsource, sizeof (struct sockaddr_in));
	if(connect(fd, (struct sockaddr*) receiver, sizeof (struct sockaddr_in)) == -1)
	{
		printf("Error connecting . . .!");
	}

	//Add it to the local list
	struct listelement* newentry = malloc(sizeof(struct listelement));
	memcpy(newentry->tubeid,spud->hdr->tubeid,8);
	newentry->receiver = receiver;
	newentry->status = 2;
	newentry->fd = fd;

	//TCP TOUPLE .  . . ev. nachtragen wenn erstes Data Packet ankommet.

	spud->hdr->cmdflags |= 1<<7;
	spud->hdr->cmdflags |= 1<<6;  //set to 0b110000;
	spud->datalenght = 0;

	sendspud(fd,spud);

	return 1;
}


int closetube(uint8_t* tubeid) //typ argument . . .
{
	//This function handles the closure of a tube initiated by someone else.
		
	//Remove from list of opentubes
	struct listelement* this = searchlist(tubeid);
	
	if(this != NULL)
	{
		close(this->fd);
		free(this->receiver);
		free(this->tcp);
		removetube(this);
	}

	return 0;
}

