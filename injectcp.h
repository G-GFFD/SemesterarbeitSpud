#define SRC_ETHER_ADDR "aa:aa:aa:aa:aa:aa"
#define DST_ETHER_ADDR "bb:bb:bb:bb:bb:bb"

typedef struct PseudoHeader{

	unsigned long int source_ip;
	unsigned long int dest_ip;
	unsigned char reserved;
	unsigned char protocol;
	unsigned short int tcp_length;

}PseudoHeader;


int CreateRawSocket(int protocol_to_sniff);
int BindRawSocketToInterface(char *device, int rawsock, int protocol);
int SendRawPacket(int rawsock, unsigned char *pkt, int pkt_len);
struct ethhdr* CreateEthernetHeader(char *src_mac, char *dst_mac, int protocol);
//int CreatePseudoHeader(struct tcphdr *tcph, struct iphdr *iph, unsigned char *data);
