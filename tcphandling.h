// tcphandling.h - provides some tcp related handling functions


//Given a buffer with an complete IP Packet, the ipheader and the tcpheader this function extracts the tcp data onto the heap and returns a pointer to it
char* extracttcpdata(void* buffer, int tcpdatalength, struct tcphdr* tcph);

//Given a buffer with a complete IP Packet and a pointer to the ipheader it extracts the tcp header with the options onto the heap and returns a pointer to it
struct tcphdr* extracttcph(void* buffer);

//Given a buffer with a complete IP Packet it extracts the iphdr with it options onto the heap and returns a pointer to it
struct iphdr* extractiph(void* buffer);

//Updates the tcp checksum of the given tcp header, needs ipheader and tcpdata too
void updatetcpchecksum(struct tcphdr* tcph, struct iphdr* iph, char* data);

//Computes TCP Checksum  (code to compute TCP checksum is borrowed from: http://www.roman10.net/2011/11/27/how-to-calculate-iptcpudp-checksumpart-2-implementation/)
unsigned short compute_tcp_checksum(struct iphdr *pIph, unsigned short *ipPayload);

//Computes IP Checksum (code to compute IP checksum is borrowed from: http://www.roman10.net/2011/11/27/how-to-calculate-iptcpudp-checksumpart-2-implementation/)
static unsigned short compute_ip_checksum(unsigned short *addr, unsigned int count);

//Updates IP Checksum
void updateipchecksum(struct iphdr* iphdrp);
