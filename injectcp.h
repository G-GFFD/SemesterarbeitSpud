//Injectcp nach tcpcrypt - Variante 3

//This Function opens the socket
void raw_open(void);

//This Function injects the packets
void raw_inject(void *data, int len);

//This function is called to inject an ip packet
int injectcp(struct iphdr* iph, struct tcphdr* tcph, void* tcpdata);

