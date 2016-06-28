// Injecttcp nach Beispiel

#include "injectcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <features.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <arpa/inet.h>


int injecttcp(struct iphdr* iph, struct tcphdr* tcph, void* tcpdata)
{
	int raw;
	unsigned char *packet;
	struct ethhdr* ethernet_header;
	int pkt_len;

	int tcpdatalength = ntohs((iph->tot_len))-((int)(tcph->doff)+((int)iph->ihl))*4;
	
	raw = CreateRawSocket();

	BindRawSocketToInterface("eth0", raw, ETH_P_ALL);

	ethernet_header = CreateEthernetHeader(SRC_ETHER_ADDR, DST_ETHER_ADDR, ETHERTYPE_IP);

	//Packet length
	pkt_len = ntohs(iph->tot_len)+sizeof(struct ethhdr);//1000;//ntohs(iph->tot_len);

		packet = (unsigned char *)malloc(pkt_len/*+sizeof(struct ethhdr)*/);

		//Copy Ethernet Header
		memcpy(packet, ethernet_header, sizeof(struct ethhdr));

		//Next Copy IPHDR
		memcpy((packet + sizeof(struct ethhdr)), iph, (iph->ihl)*4);

		//Copy TCPHDR
		memcpy((packet + sizeof(struct ethhdr) + (iph->ihl)*4),tcph, (tcph->doff)*4);
	
		//Finally copy Data
		memcpy((packet + sizeof(struct ethhdr) + (iph->ihl)*4 + (tcph->doff)*4), tcpdata, tcpdatalength /*1500-sizeof(struct ethhdr)-(iph->ihl)*4- (tcph->doff)*4*/);
	
		if(!SendRawPacket(raw, packet, pkt_len))
		{
			printf("Error sending packet due to error %s \n", strerror(errno));
		}

		else
		{
			printf("TCP Injected!\n");
		}

		free(packet);



	free(ethernet_header);
	free(iph);
	free(tcph);
	free(tcpdata);
	

	close(raw);

	return 0;
}


int CreateRawSocket()
{
	int rawsock;

	if((rawsock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL)))== -1)
	{
		printf("Error creating raw socket! Errno: %s", strerror(errno));
		exit(-1);
	}

	return rawsock;
}

int BindRawSocketToInterface(char *device, int rawsock, int protocol)
{
	struct sockaddr_ll sll;
	struct ifreq ifr;

	bzero(&sll, sizeof(sll));
	bzero(&ifr, sizeof(ifr));
	
	strncpy((char *)ifr.ifr_name, device, IFNAMSIZ);
	if((ioctl(rawsock, SIOCGIFINDEX, &ifr)) == -1)
	{
		printf("Error getting Interface index !\n");
		exit(-1);
	}


	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htons(protocol);

	if((bind(rawsock, (struct sockaddr *)&sll, sizeof(sll)))== -1)
	{
		perror("Error binding raw socket to interface\n");
		exit(-1);
	}

	return 1;
	
}

int SendRawPacket(int rawsock, unsigned char *pkt, int pkt_len)
{
	int sent= 0;

	if((sent = write(rawsock, pkt, pkt_len)) != pkt_len)
	{
		/* Error */
		printf("Could only inject %d bytes of packet of length %d\n", sent, pkt_len);
		return 0;
	}

	return 1;
}

struct ethhdr* CreateEthernetHeader(char *src_mac, char *dst_mac, int protocol)
{
	struct ethhdr *ethernet_header;

	ethernet_header = (struct ethhdr *)malloc(sizeof(struct ethhdr));

	// copy mac adresses
	memcpy(ethernet_header->h_source, (void *)ether_aton(src_mac), 6);
	memcpy(ethernet_header->h_dest, (void *)ether_aton(dst_mac), 6);

	// set protocol to tcp
	ethernet_header->h_proto = htons(protocol);

	return (ethernet_header);
}
