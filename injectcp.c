#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<linux/if_packet.h>
#include<linux/ip.h>
#include<linux/tcp.h>
#include<arpa/inet.h>
#include<string.h>
#include<sys/time.h>
#include<sys/types.h>
#include<netinet/in.h>

#include "injectcp.h"

#define DST_IP	"10.2.116.90"
#define DST_PORT	8000


static int _s;

void raw_open()
{       
        int one = 1;

        _s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
        if (_s == -1)
                err(1, "socket()");

        if (setsockopt(_s, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one))
	    == -1)
                err(1, "IP_HDRINCL");

	printf("Socket is now open");
}

void raw_inject(void *data, int len)
{
        int rc;
        struct iphdr *iph = data;
        struct tcphdr *tcp = (struct tcphdr*) ((char*) iph + (iph->ihl << 2));
        struct sockaddr_in s_in;

	

	memset(&s_in, 0, sizeof(s_in));

        s_in.sin_family = PF_INET;
        /*s_in.sin_addr   =  */inet_aton(DST_IP, &s_in.sin_addr); //(struct in_addr) iph->daddr;
        s_in.sin_port   = tcp->dest;

	printf("Sequence Nr. of TCP about to be injected %i \n", tcp->seq);
	printf("Ack Nr. of TCP about to be injected %i \n", tcp->ack);

        rc = sendto(_s, data, len, 0, (struct sockaddr*) &s_in,
		    sizeof(s_in));

        if (rc == -1)
                err(1, "sendto(raw)");

        if (rc != len)
                errx(1, "wrote %d/%d", rc, len);

	printf("sucessful injection, wrote %i bytes\n", rc);
}
 
injectcp(struct iphdr* iph, struct tcphdr* tcph, void* tcpdata)
{

	//unsigned char *packet;
	int pkt_len;

	//schauen das nur 1x aufgemacht wird
	raw_open();

	pkt_len = ntohs(iph->tot_len);
	void* packet = (unsigned char *)malloc(pkt_len);
	int data_len = ntohs(iph->tot_len)-(iph->ihl+tcph->doff)*4;

	//printf("Total len of ip packet to be injected is %i bytes \n", pkt_len);
	//printf("Datalen of packet to be injected is %i bytes \n", data_len);
	//fflush(stdout);

	memcpy(packet, iph, iph->ihl*4);

	memcpy((packet + iph->ihl*4),tcph, tcph->doff*4);

	memcpy((packet + iph->ihl*4 + tcph->doff*4), tcpdata, data_len);

	raw_inject(packet, pkt_len);


	free(iph);
	free(tcph);
	free(tcpdata);
	free(packet);

	//Nicht jedesmal closen
	close(_s);

	return 0;
}


