#include "tubelist.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

void addtube(struct listelement* next)
{
	sem_wait(&s);

	next->previous = current;
	
	if(current!=NULL)
	{
		current->next = next;
	}

	current = next;

	current->tcpfinseen = 0; // initialize to 0
	current->timeout = 6;

	sem_post(&s);
		
}

void removetube(struct listelement* old)
{
	sem_wait(&s);
	
	if(old == NULL) return;
	
	//Case 1: Element in der Mitte der Liste
	if(old->previous != NULL && old->next != NULL)
	{
		old->previous->next = old->next;
		old->next->previous = old->previous;
	}
	
	//Case 3: first Element of the list at the beginning
	else if(old->previous == NULL && old->next != NULL)
	{
		old->next->previous = NULL;
	}

	//case 3: latest, most recent Element of the list

	else if(old->previous != NULL && old->next == NULL)
	{
		current = old->previous;
	}

	//Element löschen
	close(old->fd);
	free(old->receiver);
	free(old->tcp);
	free(old);
	
	sem_post(&s);
}

struct listelement* searchlist(uint8_t* tubeid)
{
	
	if(current != NULL)
	{
		sem_wait(&s); //Semaphore so that Element temp is not unexpectedly removed by an other process

		struct listelement* temp = current; // current must always point to the last element in the list
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
				sem_post(&s);
				return temp;
			}

			temp = temp->previous;
		}

		sem_post(&s);
	}
	
		return NULL; // Aussen überprüfen, falls 0 zurück dann wurde nicht gefunden
}


uint8_t* findtcptuple(struct tcptuple* tcp)
{
	sem_wait(&s); //Semaphore so that Element temp is not unexpectedly removed by an other process

	if(current != NULL)
	{
		struct listelement* temp = current; // current points always to the last element

		do
		{
			if(comparetcptuple(temp->tcp, tcp))
			{
				sem_post(&s);
				return temp->tubeid;
			}
			temp = temp->previous;
		}while(temp != NULL);
	}
	//Not Found:

	sem_post(&s);
	return NULL;

}

int comparetcptuple(struct tcptuple* a, struct tcptuple* b)
{
	if(a->srcip == NULL || b->srcip == NULL || a->destip == NULL || b->destip == NULL)
	{
		//debug . ..
		printf("Error, one of the ip pointers in the tcptouple is NULL\n");
		return 0;
	}

	if(a->srcport == b->srcport && a->destport==b->destport && strcmp(a->srcip,b->srcip)==0 && strcmp(a->destip,b->destip)==0) return 1;

	return 0;
}
