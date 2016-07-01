// Implementation of a linked list used to manage open tube list in spud.c
#include <stdint.h>
#include <stddef.h>
#include <semaphore.h>

struct listelement
{

	struct listelement* next; //pointer to next listelement
	struct listelement* previous;
	
	uint8_t tubeid[8];
	struct tcptuple* tcp;
 	struct sockaddr_in* receiver;
	uint8_t status;

	int fd;
	int tcpfinseen;
	int finseqnr;
	int timeout;
	
	
}listelement;

struct tcptuple
{
	char* srcip;
	int srcport;

	char* destip;
	int destport;
}tcptuple;

struct listelement* current;
sem_t s;

//This Function adds a new Element to the List
void addtube(struct listelement* next);

//This Function removes an Element from the List
void removetube(struct listelement* old);

//This Function returns the Listelement to the tubeid or, if not found, NULL
struct listelement* searchlist(uint8_t* tubeid);

//This Function returns the Tubeid of a tcptuple if it is in the list, or NULL
uint8_t* findtcptuple(struct tcptuple* tcp);

//This Function 1 if tcptuple are identical, else 0
int comparetcptuple(struct tcptuple* a, struct tcptuple* b);
