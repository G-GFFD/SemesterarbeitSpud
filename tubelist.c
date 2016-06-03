#include "tubelist.h"
#include <stddef.h>

void addtube(struct listelement* next)
{
	//printf("adding a new element to the list");
	next->previous = current;
	
	if(current!=NULL)
	{
		current->next = next;
	}

	current = next;
		
}

void removetube(struct listelement* old)
{
	
	if(old == NULL) return;
	
	//Case 1: Element in der Mitte der Liste
	if(old->previous != NULL)
	{
		old->previous->next = old->next;
		old->next->previous = old->previous;
	}
	
	//Case 2: letztes Element
	else
	{
		old->next->previous = NULL;
	}

	//Element löschen
	free(old);
}

struct listelement* searchlist(uint8_t* tubeid)
{
	struct listelement* temp = current; //to not manipulate current position pointer
	int i, t;
	
	while(temp->previous != NULL)
	{
		t = 0;

		//This block is to check if 8 * 8bits of the Tubeid match . . .
		for(i=0; i<8; i++)
		{
			if(tubeid[i]!=temp->tubeid[i])
			{
				t = 1; // not matching;
			}

		}

		if(t==0)
		{
			//tubeid matches
			return temp;
		}

		temp = temp->next;
	}

	return NULL; // Aussen überprüfen, falls 0 zurück dann wurde nicht gefunden
}


uint8_t* findtcptuple(struct tcptuple* tcp)
{
	struct listelement* temp = current; //to not manipulate current position pointer
	int i;

	if(temp != NULL)
	{

		while(temp->previous != NULL)
		{
			if(comparetcptuple(temp->tcp, tcp))
			{
				//printf("find tcp found something!");
				return temp->tubeid;
			}
			temp = temp->previous;
		}
	}
	//Not Found:
	return NULL;

}

int comparetcptuple(struct tcptuple* a, struct tcptuple* b)
{
	if(a->srcport == b->srcport && a->destport==b->destport && strcmp(&a->srcip,&b->srcip)==0 && strcmp(&a->destip,&b->destip)==0) return 1;

	return 0;
}
