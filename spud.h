// Implementation of Simple SPUD without CBOR - Headerfile
#include <stdint.h>
#include "tubelist.h"

struct spudheader
{
	uint8_t magic[4];// = 0xd8000d8;
	uint8_t tubeid[8]; //64bit gross machen!
	uint8_t cmdflags; // 2bits cmd, 2bit flags, 4 bits reserved 0000
	
} spudheader;

struct spudpacket
{
	struct spudheader* hdr;
	void* data; //zeiger auf daten (enthält iphdr, tcphdr, tcpdata)
	int datalenght; // grösse tcp daten
	
} spudpacket;

//This Function generates a random 64bit Tubeid and sets it into the spud header
int IdGenerator(uint8_t* tubeid);

//This Function opens a new Tube to a Receiver, adds it to the local list and sends the open spud packet
int OpenNewTube(struct sockaddr_in* receiver, struct tcptuple* tcp);

//This Function creates and returns a spudpacket out of a tubeid, iphdr, tcphdr and tcpdata
struct spudpacket* CreateSPUD(uint8_t* tubeid, struct iphdr *iph, struct tcphdr *tcph, void* tcpdata);

//This Function sends the SPUD Packet to the receiver (here called other)
int SendSPUD(struct spudpacket* spud);

//This Function handles received spud packets according to its cmdflags
int HandleReceivedPacket(struct spudpacket* spud, struct sockaddr_in* receiver);

//This Function generates a SPUD Packet to acknowledge the request to open a tube
int AcknowledgeNewTube(struct spudpacket* spud, struct sockaddr_in* receiver);

//Called after a packet with close cmd is received, removes tube from list of opentubes
int CloseTube(uint8_t* tubeid);

//This Function allocates space for the SPUD Header and initializes it with standart Values
struct spudheader* MallocSpudHeader();

// After the last ACK has been sent, close everything
int FinishTubeClosure(uint8_t* tubeid, struct iphdr *iph, struct tcphdr * tcph, void* tcpdata);

// FIN flag detected, prepare Tube closure
int InitiateTubeClosure(uint8_t* tubeid, struct iphdr *iph, struct tcphdr *tcph, void* tcpdata);
