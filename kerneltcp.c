#include <linux/module.h>
#include <linux/kernel.h>

#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

//Additional Headers for Netfilter

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

#include <linux/udp.h>
#include <linux/ip.h>

// Header TCP
#include <linux/tcp.h>


#include <linux/slab.h>
 
// Netfilter Part
static struct nf_hook_ops nfho;   //net filter hook option struct
struct sk_buff *sock_buff;
struct iphdr *ip_header;            //ip header struct
  

#define NETLINK_USER 31

// Netlink Part
struct sock *nl_sk = NULL;


int session=0;
int pid; // Global, damit verfuegbar zum senden

/*struct packet {
	struct iphdr iph;
	struct tcphdr tcph;
	
};*/



static void startsession(struct sk_buff *skb) {

	//umarbeiten in received command from userspace, pid hinzufügen & removen etc.

	struct nlmsghdr *nlh;
	
	printk(KERN_INFO "Entering: %s\n", __FUNCTION__); // Erste Nachricht aus Userspace erhalten, 

	nlh=(struct nlmsghdr*)skb->data;

	pid = nlh->nlmsg_pid; /*pid of sending process  <----------------------- ausserhalb 1. Nachricht empfangen, pid global speichern*/
	
	session = 1;

	printk(KERN_INFO "startsession successful");

	// Session is now active (Todo: How to detect when userspace application has closed?!)
}



static void sendtouserspace(struct sk_buff *skb) {

	printk(KERN_INFO "Entering: %s\n", __FUNCTION__);
	

	if(session == 1)
	{
	
		//struct packet *mypacket;
		struct iphdr *iph;          /* IPv4 header */
   		struct tcphdr *tcph;        /* TCP header */

		iph = ip_hdr(skb); 
		tcph = tcp_hdr(skb);

		 unsigned char *user_data;   /* TCP data begin pointer */
   		 unsigned char *tail;        /* TCP data end pointer */
   		 unsigned char *it;          /* TCP data iterator */
		
		struct sk_buff *skb_out;
		struct nlmsghdr *nlh;
		
		//Test ob es bei char messages auch crasht

		skb_out = nlmsg_new(sizeof(struct iphdr),0);

		if(!skb_out)
		{
	 		printk(KERN_ERR "SKB empty!\n");
   			return;
		} 
	
		nlh=nlmsg_put(skb_out,0,0,NLMSG_DONE,sizeof(struct iphdr),0);  
		NETLINK_CB(skb_out).dst_group = 0; // not in mcast group

		memcpy(nlmsg_data(nlh),iph,sizeof(struct iphdr));

		printk(KERN_INFO "About to send iphdr to userspace\n");

		int res;

		res=nlmsg_unicast(nl_sk,skb_out,pid);

		// ev.
		if(res<0)
		{
    			printk(KERN_INFO "Error while sending bak to user\n");
		}
		
		else
		{
			printk(KERN_INFO "Sucessfully sent skbuff to user\n");
		}

		//free(skb_out);

		skb_out = nlmsg_new(sizeof(struct tcphdr),0);

		if(!skb_out)
		{
	 		printk(KERN_ERR "SKB empty!\n");
   			return;
		} 
	

		//Send tcphdr to userspace
		nlh=nlmsg_put(skb_out,0,0,NLMSG_DONE,sizeof(struct tcphdr),0);  
		NETLINK_CB(skb_out).dst_group = 0; // not in mcast group
		memcpy(nlmsg_data(nlh),tcph,sizeof(struct tcphdr));
		res=nlmsg_unicast(nl_sk,skb_out,pid);

		//Send tcpdata to userspace

		//Size TCP Data == Endpointer-Startpointer . . .
		//int tcpsize = tcph.tail - tcph_user_data;

		//void *ptr = kmalloc(tcpsize, GFP_KERNEL);

		//memcpy(ptr,tcph_user_data, tcpsize);

		char *data;

		printk(KERN_INFO "IPHEADER Length: %i\n", (int)iph->ihl);
		data = (char*)((unsigned char*)tcph+(tcph->doff*4));
		printk(KERN_INFO "TCP Header pointer starts at %p\n", tcph);
		printk(KERN_INFO "TCP Data starts at %p\n", data);
		printk(KERN_INFO "TCP Data offset is %i\n", tcph->doff);
		

		//printk(KERN_INFO "TCP DATA \n\n %s \n", *data);
		//data = "testestest";

		//Printing IPHeader data length:
		//printk(KERN_INFO "IPHDR len: %i",(int)iph->tot_len);
		
		int datalength = (int)(iph->tot_len)-((int)(tcph->doff)+((int)iph->ihl))*4;
		printk(KERN_INFO "TCP Data lenght: %i bytes", datalength);
		//int datalenght = (int)(iph->tot_len)-((iph->
		//altes skb_out freigebene?!

		skb_out = nlmsg_new(datalength,0);
		nlh=nlmsg_put(skb_out,0,0,NLMSG_DONE,datalength,0);  
		NETLINK_CB(skb_out).dst_group = 0; // not in mcast group
		printk(KERN_INFO "IP tot_len is: %i\n", iph->tot_len);
		printk(KERN_INFO "tcp->doff is: %i\n", tcph->doff);
		printk(KERN_INFO "Datalenght is %i\n",datalength);
		memcpy(nlmsg_data(nlh),data,datalength);
		//strncpy(nlmsg_data(nlh), data, datalength/4/*strlen(data)*/);		
		res=nlmsg_unicast(nl_sk,skb_out,pid);	
		

		printk(KERN_INFO "sent to userspace completed\n");
	}
	
	else
	{
		printk(KERN_INFO "session not active!\n");
	}


}

static void messagereceived(struct sk_buff *skb){

	printk(KERN_INFO "Netlink Message received\n");
	startsession(skb);
}


unsigned int hook_func(unsigned int hooknum, struct sk_buff **skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	printk(KERN_INFO "netfilter hook function\n");

	sock_buff = skb; 
	if(!sock_buff) { 
		printk(KERN_INFO "sock_buff not true\n"); // Problem sock_buff always false!
	return NF_ACCEPT;
	}
	
	printk("before ipheader");
        ip_header = (struct iphdr *)skb_network_header(sock_buff);    //grab network header using accessor
       	printk("after ipheader");
        
	struct sk_buff *skb_out = sock_buff;
 
       if (ip_header->protocol==6) {
                printk(KERN_INFO "tcp packet erhalten\n");
                //printk(KERN_INFO "got udp packet \n");     //log we’ve got udp packet to /var/log/messages
		sendtouserspace(skb_out); //ganzes struck senden, wird im userspace verarbeitet - call by reference oder by value?! Klappt das wenn pointer übergeben wird?!
                return NF_DROP;
        }
               
        return NF_ACCEPT;
}

int init_module(void) {

	printk("init stealtcp%s\n");


	//Netlink Part
	struct netlink_kernel_cfg cfg = {
  	  .input = messagereceived, // auf startsession wechseln!
	};

	nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);

	if(!nl_sk)
	{
	    printk(KERN_ALERT "Error creating socket.\n");
	    return -10;
	}

	printk(KERN_ALERT "Netlink socket created!\n");

	//Netfilter Part
	nfho.hook = hook_func;
        nfho.hooknum = NF_INET_PRE_ROUTING;
        nfho.pf = PF_INET;
        nfho.priority = NF_IP_PRI_FIRST;
 
        nf_register_hook(&nfho);

	printk(KERN_ALERT "Netfilter hook active!\n");

return 0;
}

void cleanup_module(void) {

printk(KERN_INFO "stealtcp_exit\n");

//Netlink Part
netlink_kernel_release(nl_sk);

//Netfilter Part
nf_unregister_hook(&nfho);

}

//module_init(stealtcp_init); 
//module_exit(stealtcp_exit);


