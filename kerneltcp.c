//This Kernel Moduel filters TCP packet via Netfilter and sends them to Userspace via Netlink

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
int pid;


static void startsession(struct sk_buff *skb)
{

	//Receives init Message from Userspace Application, starts to send captured TCP packets to this PID
	printk(KERN_ERR "Message from Userspace received");

	struct nlmsghdr *nlh;

	nlh=(struct nlmsghdr*)skb->data;

	pid = nlh->nlmsg_pid; // Extract & save PID
	
	session = 1;

	// Session is now active (Todo: How to detect when userspace application has closed?!)
}



static void sendtouserspace(struct sk_buff *skb)
{
	if(session == 1)
	{
		struct iphdr *iph;          /* IPv4 header */

		iph = ip_hdr(skb); 
		
		struct sk_buff *skb_out;
		struct nlmsghdr *nlh;
		skb_out = nlmsg_new(ntohs(iph->tot_len)/*iph->tot_len*/,0);

		if(!skb_out)
		{
	 		printk(KERN_ERR "Buffer is empty!\n");
   			return;
		} 
	
		nlh=nlmsg_put(skb_out,0,0,NLMSG_DONE,ntohs(iph->tot_len),0);  
		NETLINK_CB(skb_out).dst_group = 0;

		memcpy(nlmsg_data(nlh),iph,ntohs(iph->tot_len));

		int tmp;
		
		if((tmp = nlmsg_unicast(nl_sk,skb_out,pid)) <0)
		{
    			printk(KERN_INFO "Error while sending back to user\n");
		}
		
		else
		{
			printk(KERN_INFO "Sucessfully sent skbuff to user\n");
		}

		//kfree(skb_out); don't free this or kernel panic happens!

	}
	
	else
	{
		printk(KERN_INFO "session not active!\n");
	}


}


unsigned int hook_func(unsigned int hooknum, struct sk_buff **skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	sock_buff = skb; 

	if(!sock_buff)
	{ 
		printk(KERN_INFO "sock_buff not true\n");
		return NF_ACCEPT;
	}
	
        ip_header = (struct iphdr *)skb_network_header(sock_buff);
        
	struct sk_buff *skb_out = sock_buff;
 
	if (ip_header->protocol==6)
	{
		// TCP Packet detected
		sendtouserspace(skb_out);
                return NF_DROP;
        }
               
        return NF_ACCEPT;
}

int init_module(void) 
{
	//Netlink Part
	struct netlink_kernel_cfg cfg = {
  	  .input = startsession,
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
        nfho.hooknum = NF_INET_POST_ROUTING;//NF_INET_PRE_ROUTING; // ev. auf Post-routing ändern, damit iptables die tcp mss ändern konnte . . .
        nfho.pf = PF_INET;
        nfho.priority = NF_IP_PRI_FIRST;
 
        nf_register_hook(&nfho);

	printk(KERN_ALERT "Netfilter hook active!\n");

	return 0;
}

void cleanup_module(void)
{

//Netlink Part
netlink_kernel_release(nl_sk);

//Netfilter Part
nf_unregister_hook(&nfho);

}


