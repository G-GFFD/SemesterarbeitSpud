//This File provides functions to inject an IP packet, it's done by wrapping it into an Ethernet Packet and sending it via a raw socket to the Interface

#define SRC_ETHER_ADDR "08:00:27:9b:8f:70"//"aa:aa:aa:aa:aa:aa"
#define DST_ETHER_ADDR "08:00:27:9b:8f:70"//"bb:bb:bb:bb:bb:bb"

//Creates a RawSocket
int CreateRawSocket();

//Binds the socket to the specified interface
int BindRawSocketToInterface(char *device, int rawsock, int protocol);

//This Function Injects the TCP Packet via the Raw Socket
int SendRawPacket(int rawsock, unsigned char *pkt, int pkt_len);

//Creates the Ethernet Header
struct ethhdr* CreateEthernetHeader(char *src_mac, char *dst_mac, int protocol);
