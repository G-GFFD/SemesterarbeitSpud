// Implementation of Simple SPUD without CBOR - Headerfile
#include <stdint.h>

struct spudheader
{
	uint8_t magic;// = 0xd8000d8;
	uint8_t tubeid[8]; //64bit gross machen!
	uint8_t cmdflags; // 2bits cmd, 2bit flags, 4 bits reserved 0000
	
} spudheader;

struct spudpacket
{
	struct spudheader* hdr;
	void* data; //zeiger auf daten (enthält iphdr, tcphdr, tcpdata)
	int datalenght; // grösse tcp daten
	
} spudpacket;

struct opentubes
{
	uint8_t tubeid[8];
	uint8_t status; // states as defined in RFC: 0 unknown 1 opening 2 running
	struct sockaddr_in *receiver; 
} opentubes;



int idgenerator(uint8_t* tubeid);
int opennewtube(struct sockaddr_in* receiver);
struct spudpacket* createspud(uint8_t tubeid, struct iphdr *iph, struct tcphdr *tcph, void* tcpdata);
int sendspud(struct sockaddr_in* other, struct spudpacket* spud);
int handlereceivedpacket(struct spudpacket* spud, struct sockaddr_in* receiver);
int acknowledgenewtube(struct spudpacket* spud, struct sockaddr_in* receiver);
int closetube(struct spudpacket* spud, struct sockaddr_in* receiver);
uint8_t gettubeid(struct sockaddr_in* receiver);
struct sockaddr_in getreceiver(uint8_t* tubeid);
struct spudheader* mallocspudheader();
