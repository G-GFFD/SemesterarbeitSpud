// Implementation of Simple SPUD without CBOR
#include <stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<linux/ip.h>
#include<linux/tcp.h>
#include<netinet/in.h>
#include "injectcp.h"
#include "spud.h"

// need better way to manage opentubes, maybe a linked list?
struct opentubes tubes[100];
int counter = 0;

struct spudheader* mallocspudheader()
{

	//This Function allocates space for the SPUD Header and initializes it with standart Values

	struct spudheader* hdr = malloc(sizeof(struct spudheader));
	
	hdr->magic = 0xd8000d8;
	hdr->cmdflags = 0;

	return hdr;
}

int idgenerator(uint8_t* tubeid)
{
	//This Function generates a random 64bit Tubeid and sets it into the spud header

	srand(time(NULL));
	int i = 0;
	for(i<4; i++;)
	{
		memcpy(tubeid+sizeof(uint8_t)*i,(uint8_t) rand(), sizeof(uint8_t));
	}
	
}

int opennewtube(struct sockaddr_in* receiver)
{

	//This Function opens a new Tube to a Receiver, adds it to the local list and sends the open spud packet

	if(counter < 100)
	{

		//Open new Tube
		struct spudheader* newspudheader = mallocspudheader();
	
		idgenerator(newspudheader->tubeid);
		newspudheader->cmdflags = newspudheader->cmdflags & 0b1000000;

		struct spudpacket* newspud = malloc(sizeof(struct spudpacket));
		newspud->hdr = newspudheader;
		newspud->datalenght = 0;

		//Save it in the local list of open Tubes
		memcpy(tubes[counter].tubeid,newspudheader->tubeid, 4*sizeof(uint8_t));
		tubes[counter].receiver = receiver;
		tubes[counter].status = 1;
		counter++;
		
		sendspud(receiver,newspud);	
	}
	
}

struct spudpacket* createspud(uint8_t tubeid, struct iphdr *iph, struct tcphdr *tcph, void* tcpdata)
{
	//This Function creates and returns a spudpacket out of a tubeid, iphdr, tcphdr and tcpdata

	struct spudpacket* spud = malloc(sizeof(spudpacket));
	struct spudheader* header = mallocspudheader();
	
	*header->tubeid = tubeid;
	header->cmdflags = header->cmdflags & 0b00000000;

	spud->hdr = header;

	int tcpdatalenght = (int)(iph->tot_len)-((int)(tcph->doff)+((int)iph->ihl))*4;
		
	spud->datalenght = tcpdatalenght + sizeof(struct iphdr) + sizeof (struct tcphdr);


	//This if due to crashes because of ridiculusly high datalenght which caused a crash (happend rarely)
	if(spud->datalenght < 20000)
	{
		spud->data = malloc(spud->datalenght);

		//first memcpy iphdr
		memcpy(spud->data, iph, sizeof(struct iphdr));

		//second memcpy tcphdr
		memcpy(spud->data+sizeof(struct iphdr), tcph, sizeof(struct tcphdr));

		//finally memcpy tcpdata
		memcpy(tcpdata,spud->data+sizeof(struct iphdr)+sizeof(struct tcphdr), tcpdatalenght);
	}

	else
	{
		printf("Fatal Error, TCP Datalenght too large: %i",spud->datalenght);
		//For now just returning an empty spud . . .
	}

	return spud;
	
}

struct sockaddr_in* getreceiverfromtubeid(uint8_t *tubeid)
{
	//This Function returns the receiver connected to a Tube ID in the local list of Open Tubes
	

	// struct sockaddr_in other aus globaler tubeliste holen
	int i;	
	for(i=0; i< 100; i++)
	{
		if(tubes[i].tubeid == tubeid)
		{
			return tubes[i].receiver;
		}
		// wo check auf status der tube?!
	}

	struct sockaddr_in fake;
	return &fake;
}

int sendspud(struct sockaddr_in* other, struct spudpacket* spud)
{
	//This Function sends the SPUD Packet to the receiver (here called other)

	int s; // UDP Socket Descriptor

	//find out size of spudpacket & tcpdata . . .
	int size = sizeof(struct spudheader) + spud->datalenght;

	// allocate memory buf of this size . . .
	void *buf = malloc(size);
	
	//memcpy spudheader an den anfang kopieren
	memcpy(buf, spud->hdr, sizeof(struct spudheader));

	if(spud->datalenght != 0)
	{	
		//Falls daten, diese dahinter kopieren
		memcpy(buf+sizeof(struct spudheader), spud->data, spud->datalenght);
		free(spud->data);
	}
	
	//das ganze via udp senden
	s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sendto(s, buf, size, 0, &other, sizeof(other));
	printf("SPUD SENT VIA UDP\n");
	close(s);
			
	free(buf);
		
	free(spud->hdr);
	free(spud);

	return 1;
	
}

//todo
uint8_t gettubeid(struct sockaddr_in* receiver)
{
	//This Function searches the local list with the given receiver and if found returns the according tubeid
	
	//globales array durchsuchen . . .
	int i = 0;
	for (i=0; i < 100; i++)
	{
		//daddr und ports auch vergleichen. reicht das?
		if(0/*receiver->sin_addr.s_addr == tubes[i].receiver->sin_addr.s_addr*/)
		{
			return tubes[i].tubeid;
		}
	}

	return -1;
}
int handlereceivedpacket(struct spudpacket* spud, struct sockaddr_in* receiver)
{
	//This Function handles received spud packets according to its cmdflags

	if(spud->hdr->magic == 0xd8000d8)
	{
		// ok valid spud packet

		if(spud->hdr->cmdflags & 0b01000000 == 1)
		{
			//received spud wants to open a new tube
			printf("Received SPUD who wants to open a new tube\n");
			acknowledgenewtube(spud, receiver);
		}

		else if(spud->hdr->cmdflags & 0b10000000 == 1)
		{
			//received spud wants to close a tube
			printf("Received SPUD who wants to close a tube\n");
			closetube(spud,receiver);
		}
		
		else if(spud->hdr->cmdflags & 0b00000000 == 1)
		{
			//received Spud containing Data

			printf("Received SPUD Data  Packet. Extracting TCP . . .\n");
	
			//fetch ip header from the whole received spud
			struct iphdr *iph = malloc(sizeof(struct iphdr));
			memcpy(iph,spud+sizeof(struct spudheader),sizeof(struct iphdr));

			//fetch tcp header from whole received spud
			struct tcphdr *tcph = malloc(sizeof(struct tcphdr));
			memcpy(tcph,spud+sizeof(struct spudheader)+sizeof(struct iphdr),sizeof(struct tcphdr));

			//extract tcpdata . . .

			int tcpdatalenght = (int)(iph->tot_len)-((int)(tcph->doff)+((int)iph->ihl))*4;
			void *tcpdata = malloc(tcpdatalenght);
			memcpy(tcpdata,spud+sizeof(struct spudheader)+sizeof(struct iphdr)+sizeof(struct tcphdr), tcpdatalenght);

			injectcp(iph, tcph, tcpdata);
			printf("Iphdr, tcphdr, tcpdata extracted from spud, injection function called. . .\n");
			free(spud);
			free(receiver);
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

	spud->hdr->cmdflags = spud->hdr->cmdflags & 0b11000000;
	spud->datalenght = 0;

	//Add it to the local list	
	*tubes[counter].tubeid = spud->hdr->tubeid;
	tubes[counter].status = 2;
	tubes[counter].receiver = receiver;
	counter++;
	
	sendspud(receiver,spud);

	return 1;
}

//2x verschiedene Varianten von closetube
//1. Gegenstelle sendet close cmd (struct spudpacket* spud, struct sockaddr_in* receiver)
//2. Dieses Programm hier will eine Tube schliesen (uint8_t *tubeid)

/*

//Diese Variante ist noch zu implementieren (local schliessung einer Tube einleiten)
int closetube(uint8_t *tubeid) //typ argument . . .
{
	printf("function closetube\n");
	struct spudheader* hdr = mallocspudheader();
	struct spudpacket* spud = malloc(sizeof(struct spudpacket));
	
	hdr->cmdflags = hdr->cmdflags & 0b10000000;
	spud->hdr = hdr;
	spud->datalenght = 0;

	struct sockaddr_in* receiver = malloc(sizeof(struct sockaddr_in));
	receiver = getreceiverfromtubeid(tubeid);
	printf("closetube calls sendspud");
	sendspud(receiver, spud);

	//to add: remove from list of open tubes
	return 0;
}*/

int closetube(struct spudpacket* spud, struct sockaddr_in* receiver) //typ argument . . .
{
	//This function closes an existing Tube if this is requested by a received SPUD Packet

	spud->hdr->cmdflags = spud->hdr->cmdflags & 0b10000000;
	spud->datalenght = 0;
	
	sendspud(receiver, spud);

	//to add: remove from list of open tubes
	return 0;
}

