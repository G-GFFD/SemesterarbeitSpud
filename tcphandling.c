#include <stdlib.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include "tcphandling.h"

struct iphdr* extractiph(void* buffer)
{
		struct iphdr* iph = malloc(sizeof(struct iphdr));
		memcpy(iph, buffer, sizeof(struct iphdr));

		if(iph->ihl > 5)
		{
			//IP Header has additional IP options behind it
			//Allocate more space and copy them behind
			iph = realloc(iph, iph->ihl * 4);
			memcpy(iph,buffer,(iph->ihl)*4);	
		}

		return iph;

}

struct tcphdr* extracttcph(void* buffer, struct iphdr* iph)
{
	struct tcphdr* tcph = malloc(sizeof(struct tcphdr));
	memcpy(tcph, buffer+(iph->ihl)*4, sizeof(struct tcphdr));

	if(tcph->doff > 5)
	{
		//There are TCP Options, allocate more space and copy them behind
		tcph = realloc(tcph,(tcph->doff) * 4);
		memcpy(tcph,buffer+(iph->ihl)*4,(tcph->doff)*4);
	}
	
	return tcph;

}

char* extracttcpdata(void* buffer, struct iphdr* iph, struct tcphdr* tcph)
{
	//Finally extract TCP Data		
	int tcpdatalenght = (int)(iph->tot_len)-((int)(tcph->doff)+((int)iph->ihl))*4;
		
	if(tcpdatalenght >0)
	{
		char* data = malloc(tcpdatalenght);
		memcpy(data, buffer+(iph->ihl)*4+(tcph->doff)*4, tcpdatalenght);
		return data;
	}
	
	else
	{
		printf("INFO: TCP Datalenght is not > 0 . . . \n");
		return NULL;
	}		

}

void updatetcpchecksum(struct tcphdr* tcph, struct iphdr* iph, char* data)
{
	//Allocate space for tcp header + tcpdata
	void* payload = malloc((iph->tot_len)-(iph->ihl)*4);

	//Copy TCP header
	memcpy(payload,tcph,(tcph->doff*4));

	//Copy TCP data just behind
	memcpy(payload+(tcph->doff*4), data, (iph->tot_len)-(iph->ihl + tcph->doff)*4);

	tcph->check = compute_tcp_checksum(iph,(unsigned short*)payload);

	free(payload);

	
}

// Following code to compute TCP checksum is borrowed from: http://www.roman10.net/2011/11/27/how-to-calculate-iptcpudp-checksumpart-2-implementation/
// (it's slightly adapted to fit in my program)

unsigned short compute_tcp_checksum(struct iphdr *pIph, unsigned short *ipPayload) 
{

    register unsigned long sum = 0;

    unsigned short tcpLen = ntohs(pIph->tot_len) - (pIph->ihl<<2);

    struct tcphdr *tcphdrp = (struct tcphdr*)(ipPayload);

    //add the pseudo header 

    //the source ip

    sum += (pIph->saddr>>16)&0xFFFF;

    sum += (pIph->saddr)&0xFFFF;

    //the dest ip

    sum += (pIph->daddr>>16)&0xFFFF;

    sum += (pIph->daddr)&0xFFFF;

    //protocol and reserved: 6

    sum += htons(IPPROTO_TCP);

    //the length

    sum += htons(tcpLen);

 

    //add the IP payload

    //initialize checksum to 0

    tcphdrp->check = 0;

    while (tcpLen > 1) {

        sum += * ipPayload++;

        tcpLen -= 2;

    }

    //if any bytes left, pad the bytes and add

    if(tcpLen > 0) {

        //printf("+++++++++++padding, %dn", tcpLen);

        sum += ((*ipPayload)&htons(0xFF00));

    }

      //Fold 32-bit sum to 16 bits: add carrier to result

      while (sum>>16) {

          sum = (sum & 0xffff) + (sum >> 16);

      }

      sum = ~sum;

    //set computation result

    return (unsigned short)sum;

}
