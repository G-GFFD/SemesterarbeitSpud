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
#include <stdio.h>
#include <string.h>

#define SERVER "10.2.115.157"
#define TubeTimeOut 6

uint8_t magic[4] = {11011000,00000000,0000000,11011000};

struct spudheader* MallocSpudHeader()
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

int IdGenerator(uint8_t* tubeid)
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

uint8_t* OpenNewTube(struct sockaddr_in* receiver, struct tcptuple* tcp)
{
	//This Function opens a new Tube to a Receiver, adds it to the local list

		uint8_t* newtubeid = malloc(8*sizeof(uint8_t));
	
		//just to make sure new tube id is not already taken
		do{
		IdGenerator(newtubeid);
		}while(searchlist(newtubeid) != NULL);
		//wihtout do-while I got multiple tubes with the same id if they were created immediatley after another, strange . . ..

		//Create Status in List of Open Tubes
		struct listelement *newentry = malloc(sizeof(struct listelement));
		memcpy(newentry->tubeid,newtubeid,8*sizeof(uint8_t));

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

		//At the moment, set fixed receiver. Later just adjust port.
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

		return newtubeid;	
	
}

struct spudpacket* CreateSPUD(uint8_t* tubeid, struct iphdr *iph, struct tcphdr *tcph, void* tcpdata)
{
	//This Function creates and returns a spudpacket out of a tubeid, iphdr, tcphdr and tcpdata

	struct spudpacket* spud = malloc(sizeof(spudpacket));
	struct spudheader* header = MallocSpudHeader();
	memcpy(header->tubeid, tubeid, 8*sizeof(uint8_t));

	//Listelement zur Tubeid finden  . . . (falls NULL problem . . .)
	//Header flags nach status setzen.
	header->cmdflags = 0;

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


int SendSPUD(struct spudpacket* spud)
{
	//SENDSPUD MUSS FD SELBER AUS LISTELEMENT HOLEN!

	struct listelement* thistube = searchlist(spud->hdr->tubeid);
	
	if(thistube == NULL)
	{
		//Problem . . .
		return 0;

	}
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

	int temp = send(thistube->fd, buf, size, 0);

	//Reset Tube Time-out
	thistube->timeout = TubeTimeOut;

	if(temp == -1)
	{
		printf("Error: %s\n", strerror(errno));
	}

	else
	{
		printf("\nSPUD sent! \n",temp);
	}
		
	free(buf);
	free(spud->hdr);
	free(spud);

	return 1;
	
}

int HandleReceivedPacket(struct spudpacket* spud, struct sockaddr_in* receiver)
{
	//This Function handles received spud packets
	
	if(memcmp(magic,spud->hdr->magic,4)==0)
	{
		// ok valid spud packet
				
		//check if bits in cmdflags are set 10xxxxx
		if((((spud->hdr->cmdflags & (1 << 7)) >> 7) &1) && (!((spud->hdr->cmdflags & (1 << 6)) >> 6) &1))
		{
			//received spud wants to close a tube
			CloseTube(spud->hdr->tubeid);
		}

		//check if bits in cmdflags are set 11xxxxx
		if((((spud->hdr->cmdflags & (1 << 6)) >> 6) &1) && (((spud->hdr->cmdflags & (1 << 7)) >> 7) &1))
		{
			//received SPUD that acknowledges an opened tube
			struct listelement* temp;
			temp = searchlist(spud->hdr->tubeid);
			
			if(temp != NULL)
			{
				temp->status = 2;
			}
			
			else
			{
				//This case shouldn't happen.
			}

		}
		
		struct iphdr *iph = NULL;
		struct tcphdr *tcph = NULL;
		char *data = NULL;

		extractiph(iph, spud->data);
		extracttcph(tcph, spud->data,iph);
		extracttcpdata(data, spud->data,iph,tcph);

		updatetcpchecksum(tcph,iph,data);

		if(iph != NULL && tcph != NULL)
		{
			injecttcp(iph, tcph, data);
		}

		else
		{
			printf("Extraction unsuccessfull, can't inject tcp\n");
		}

		free(spud->data);
		free(spud);
		free(receiver);

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


int AcknowledgeNewTube(struct spudpacket* spud, struct sockaddr_in* receiver)
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

	SendSPUD(spud);

	return 1;
}


int CloseTube(uint8_t* tubeid) //typ argument . . .
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

int FinishTubeClosure(uint8_t* tubeid, struct iphdr *iph, struct tcphdr *tcph, void* tcpdata)
{
	struct listelement* this = searchlist(tubeid);
	
	if(this != NULL)
	{
		this->status = 2; // ändern so das close spud gesendet wird
		SendSPUD(CreateSPUD(tubeid, iph, tcph, tcpdata));

		close(this->fd);
		free(this->receiver);
		free(this->tcp);
		removetube(this);
	}

	return 1;

	
}

int InitiateTubeClosure(struct listelement* this, struct iphdr *iph, struct tcphdr *tcph, void* tcpdata)
{
		//struct listelement* this = searchlist(tubeid);
		
		if(this != NULL)
		{
			this->tcpfinsent = 1;
			SendSPUD(CreateSPUD(this->tubeid, iph, tcph, tcpdata));
		}
		
		return 1;
		
}
