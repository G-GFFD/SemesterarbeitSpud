//Information: Inject Sourcecode based on Implementation of TCPCrypt

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

#define DSTIP	"10.2.116.136" //local adresse as they'll be injected on this machine
#define DST_PORT	8000

#define SRCIP "10.2.118.85"  //take UDP source instead of hardcoded address


static int _s;
struct iphdr* iph;

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

	//setup iphdr used to inject tcp packet locally

	iph = malloc(sizeof(struct iphdr));

	iph->version = 4;
	iph->tos=0;
	iph->ihl=(sizeof(struct iphdr))/4;
	iph->id = htons(111);
	iph->frag_off = 0;
	iph->ttl = 111;
	iph->protocol = IPPROTO_TCP;
	
	iph->saddr = inet_addr(SRCIP);
	iph->daddr = inet_addr(DSTIP);

	//to be adjusted individually for every injected packet
	iph->tot_len = 0;
	iph->check = 0;

}

void raw_inject(void *data, int len)
{
        int rc;
        struct iphdr *iph = data;
        struct tcphdr *tcp = (struct tcphdr*) ((char*) iph + (iph->ihl << 2));
        struct sockaddr_in s_in;

	

	memset(&s_in, 0, sizeof(s_in));

        s_in.sin_family = PF_INET;
        /*s_in.sin_addr   =  */inet_aton(DSTIP, &s_in.sin_addr); //(struct in_addr) iph->daddr;
        s_in.sin_port   = tcp->dest;

        rc = sendto(_s, data, len, 0, (struct sockaddr*) &s_in,
		    sizeof(s_in));

        if (rc == -1)
                err(1, "sendto(raw)");

        if (rc != len)
                errx(1, "wrote %d/%d", rc, len);

	printf("sucessful injection, wrote %i bytes\n", rc);
}
 
injectcp(struct tcphdr* tcph, void* tcpdata, int pkt_len)
{

	//schauen das nur 1x aufgemacht wird
	raw_open();

	//use this, in order to set the source address of the ip packet to the source of the received struct sockaddr_in iph->saddr = inet_addr(inet_ntoa(((struct sockaddr_in *)&if_ip.ifr_addr)->sin_addr)); for now, set to fixed address

	//adjust iphdr, compute checksum
	pkt_len = pkt_len + sizeof(struct iphdr);

	iph->tot_len = htons(pkt_len);
	iph->check = updateipchecksum(iph);

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


