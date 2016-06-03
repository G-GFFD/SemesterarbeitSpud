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
//#include "tubelist.h"
#define SERVER "127.0.0.1"

uint8_t magic[4] = {11011000,00000000,0000000,11011000};

struct spudheader* mallocspudheader()
{

	//This Function allocates space for the SPUD Header and initializes it with standart Values

	struct spudheader* hdr = malloc(sizeof(struct spudheader));
	
	memcpy(hdr->magic,magic,32);

	hdr->cmdflags = 0;
	
	//just to try to fix a crash caused by uninitialized values in mallocspudheader()
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
	
		idgenerator(newspudheader->tubeid);
		newspudheader->cmdflags = newspudheader->cmdflags & 0b1000000;

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
		newentry->tcp = tubetcp;
		newentry->receiver = receiver;
		newentry->status = 1;
		addtube(newentry);

		struct sockaddr_in localsource;
		int fd;
		memset(&localsource,0, sizeof(struct sockaddr_in));
		inet_aton(SERVER, &receiver->sin_addr);
		receiver->sin_port = htons(3334);
		localsource.sin_port = htons(tubetcp->srcport-1); // fragen ob das schlau ist . . .

		if ( (fd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		{
			printf("Failed to create socket!");
		}
		bind(fd, (struct sockaddr*) &localsource, sizeof (struct sockaddr_in));
		if(connect(fd, (struct sockaddr*) receiver, sizeof (struct sockaddr_in)) == -1)
		{
			printf("ERROR WHILE CONNECT");
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
		memcpy(spud->data+sizeof(struct iphdr)+sizeof(struct tcphdr), tcpdata, tcpdatalenght);
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
	//This Function sends the SPUD Packet to the receiver (here called other)

	//int s; // UDP Socket Descriptor

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

	//struct sockaddr_in me;
	//memset(&&me, 9, sizeof(struct sockaddr_in));
	//me.sin_family = AF_INET;

	//me.sin_port=htons(PORT); // sourceport, for each send queue, diffrent then receiver
	
	//das ganze via udp senden
	//s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//0printf("Socketfile descriptor is %i, -1 would indicate error", s);
	//leep(1);
	int temp = send(fd, buf, size, 0);
	//printf("SPUD SENT VIA UDP TO %s on port %i \n",inet_ntoa(other->sin_addr),ntohs(other->sin_port));
	//printf("Sento returned %i, that's the number of sent characters\n\n", temp);
	//printf("sendto Error %s",strerror(errno));
	//close(s);

	//Socket schliessen bei closetube();
	printf("Spud magic number of sent packet  . . . 0x%08x\n", ((struct spudheader*)buf)->magic);
	printf("magic numberof a spudmalloc . . . 0x%08x\n", (mallocspudheader()->magic));
	free(buf);
	free(spud->hdr);
	free(spud);

	return 1;
	
}
// nicht mehr benötigt, da es jetzt findtcptouple gibt

//todo -> falscher datentyp! 8*uint8_t für die 64bit nummber, also besser pointer?
/*uint8_t gettubeid(struct sockaddr_in* receiver)
{
	//This Function searches the local list with the given receiver and if found returns the according tubeid
	
	//globales array durchsuchen . . .
	int i;
	for (i=0; i < 100; i++)
	{
		//daddr und ports auch vergleichen. reicht das?
		//if(0receiver->sin_addr.s_addr == tubes[i].receiver->sin_addr.s_addr)
		//{
		//	return tubes[i].tubeid;
		//}
	}

	return -1;
}*/
int handlereceivedpacket(struct spudpacket* spud, struct sockaddr_in* receiver)
{
	//This Function handles received spud packets according to its cmdflags
	printf("Wrong Magic Number is %i", spud->hdr->magic);
	if(memcmp(magic,spud->hdr->magic)==0)
	{
		// ok valid spud packet
		printf("received Valid SPUD");

		if(spud->hdr->cmdflags & 0b01000000 == 1)
		{
			//received spud wants to open a new tube
			printf("Received SPUD who wants to open a new tube\n");
			//create socket for this receiver, bind
			int fd;
			fd=
			acknowledgenewtube(spud, fd);
		}

		else if(spud->hdr->cmdflags & 0b10000000 == 1)
		{
			//received spud wants to close a tube
			printf("Received SPUD who wants to close a tube\n");
			closetube(spud,receiver);
		}

		else if(spud->hdr->cmdflags & 0b11000000 == 1)
		{
			//tube status auf 2, "running" updaten
			struct listelement* temp;
			temp = searchlist(spud->hdr->tubeid);
			if(temp != NULL)
			{
				temp->status = 2;
			}

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
			//spud.datalenght hätte gesamtlänge des spudpackets

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


int acknowledgenewtube(struct spudpacket* spud, int fd)
{
	//This Function generates a SPUD Packet to acknowledge the request to open a tube

	spud->hdr->cmdflags = spud->hdr->cmdflags & 0b11000000;
	spud->datalenght = 0;

	//Add it to the local list	
	struct listelement *newentry = malloc(sizeof(struct listelement));
	memcpy(newentry->tubeid,spud->hdr->tubeid,8*sizeof(uint8_t));
	
	//newentry->tcp = tcp; TCP Tuple problem noch lösen!!!
	//newentry->receiver = receiver;

	newentry->status = 2;

	
	sendspud(fd,spud);

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

	free(receiver);
	//Remove from list of opentubes
	//liste nach spud->hdr->tubeid durchsuchen, entfernen
	free(spud);
	return 0;
}

