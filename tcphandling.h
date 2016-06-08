// tcphandling.h - provides some tcp related handling functions


char* extracttcpdata(void* buffer, struct iphdr* iph, struct tcphdr* tcph);
struct tcphdr* extracttcph(void* buffer, struct iphdr* iph);
struct iphdr* extractiph(void* buffer);
void updatetcpchecksum(struct tcphdr* tcph, struct iphdr* iph, char* data);
unsigned short compute_tcp_checksum(struct iphdr *pIph, unsigned short *ipPayload);
static unsigned short compute_ip_checksum(unsigned short *addr, unsigned int count);
void updateipchecksum(struct iphdr* iphdrp);
