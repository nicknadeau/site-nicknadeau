#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <ifaddrs.h>
#include <time.h>
#include <net/if_arp.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>


__attribute__((const)) static inline size_t sll_len(const size_t halen)
{
	const struct sockaddr_ll unused;
	const size_t len = offsetof(struct sockaddr_ll, sll_addr) + halen;

	if (len < sizeof(unused))
		return sizeof(unused);
	return len;
}

static void print_hex(unsigned char *p, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		printf("%02X", p[i]);
		if (i != len - 1) {
			printf(":");
		}
	}
}

static bool recvArpPacket(int socketFd, struct sockaddr_ll *us, struct in_addr *senderIp, struct in_addr *destinationIp) {
	unsigned char packet[4096];

	struct sockaddr_storage from;
	memset(&from, 0, sizeof(from));
	socklen_t addr_len = sizeof(from);

	ssize_t bytesRead = recvfrom(socketFd, packet, sizeof(packet), 0, (struct sockaddr *) &from, &addr_len);
	if (-1 == bytesRead) {
		fprintf(stderr, "Error reading packet from socket (errno=%d)\n", errno);
		exit(1);
	}

	struct sockaddr_ll *fromLL = (struct sockaddr_ll *) &from;

	// Filter our any packets that aren't for us.
	if ((fromLL->sll_pkttype != PACKET_HOST) && (fromLL->sll_pkttype != PACKET_BROADCAST) && (fromLL->sll_pkttype != PACKET_MULTICAST)) {
		fprintf(stderr, ">> 1\n");
		return false;
	}

	struct arphdr *arpHeader = (struct arphdr *) packet;

	// We are only looking for an ARP reply.
	if (arpHeader->ar_op != htons(ARPOP_REPLY)) {
		fprintf(stderr, ">> 2\n");
		return false;
	}

	/* ARPHRD check and this darned FDDI hack here :-( */
	if ((arpHeader->ar_hrd != htons(fromLL->sll_hatype)) && (fromLL->sll_hatype != ARPHRD_FDDI || arpHeader->ar_hrd != htons(ARPHRD_ETHER))) {
		fprintf(stderr, "Hit me ??\n");
		fprintf(stderr, ">> 3\n");
		return false;
	}

	// The protocol must be IP otherwise this isn't the right packet.
	if (arpHeader->ar_pro != htons(ETH_P_IP)) {
		fprintf(stderr, ">> 4\n");
		return false;
	}

	// If the length of the protocol address is not 4 bytes then this is not an IPv4 packet, so it's not for us.
	if (arpHeader->ar_pln != 4) {
		fprintf(stderr, ">> 5\n");
		return false;
	}

	// If the length of the hardware (MAC) address is not equal to our MAC length, then this packet is not for us.
	if (arpHeader->ar_hln != us->sll_halen) {
		fprintf(stderr, ">> 6\n");
		return false;
	}

	// Ensure that the packet is correctly sized. It should be the ARP header size plus two IPv4 & MAC addresses (for sender and destination).
	if (bytesRead < ((ssize_t) (sizeof(struct arphdr) + 2 * (4 + arpHeader->ar_hln)))) {
		fprintf(stderr, ">> 7\n");
		return false;
	}

	unsigned char *payload = (unsigned char *)(arpHeader + 1);
	struct in_addr srcIp;
	struct in_addr dstIp;

	// Copy the 4 bytes of the MAC address into srcIp.
	memcpy(&srcIp, payload + arpHeader->ar_hln, 4);

	// Copy the 4 bytes of the next MAC address into dstIp.
	memcpy(&dstIp, payload + arpHeader->ar_hln + 4 + arpHeader->ar_hln, 4);

	// If the sender is not our destination, then this is not a reply from the expected machine.
	if (srcIp.s_addr != destinationIp->s_addr) {
		fprintf(stderr, ">> 8\n");
		return false;
	}

	// If the destination is not our ip, then this is not a reply meant for us.
	if (senderIp->s_addr != dstIp.s_addr) {
		fprintf(stderr, ">> 9\n");
		return false;
	}
	
	// If the target MAC address is not our MAC then this is not a reply meant for us.
	if (0 != memcmp(payload + arpHeader->ar_hln + 4, us->sll_addr, arpHeader->ar_hln)) {
		fprintf(stderr, ">> 10\n");
		return false;
	}

	printf("ARP reply from %s [", inet_ntoa(srcIp));
	print_hex(payload, arpHeader->ar_hln);
	printf("]\n");

	return true;
}

static void sendArpPacket(int socketFd, struct sockaddr_ll *us, struct sockaddr_ll *them, struct in_addr *senderIp, struct in_addr *destinationIp) {
	// We use a backing character buffer to store our ARP packet in. Since the ARP header comes first, and then the variable-length portion of it is next. The payload is
	// not specified by the header and needs to be constructed by us, at the end of the header.
	unsigned char backingBuffer[256];
	struct arphdr *arpHeader = (struct arphdr *) backingBuffer;
	unsigned char *payload = (unsigned char *)(arpHeader + 1);

	// Set the hardware address type.
	arpHeader->ar_hrd = htons(us->sll_hatype);

	// If the hardware type is fiber, then we explicitly set it back to 10/100Mbps ethernet as our hardware address format.
	if (arpHeader->ar_hrd == htons(ARPHRD_FDDI)) {
		arpHeader->ar_hrd = htons(ARPHRD_ETHER);
	}

	// Set the protocol format we will use to the IP protocol.
	arpHeader->ar_pro = htons(ETH_P_IP);

	// Set the length of the hardware (MAC) address.
	arpHeader->ar_hln = us->sll_halen;

	// Set the length of the protocol address. We are using Ipv4, so it is 4 bytes.
	arpHeader->ar_pln = 4;

	// Set the ARP opcode. This is going to be an ARP request.
	arpHeader->ar_op = htons(ARPOP_REQUEST);

	// Construct the payload. The if_arp.h file tells us the format of this portion:
	// unsigned char __ar_sha[ETH_ALEN];        // Sender hardware address.
	// unsigned char __ar_sip[4];                // Sender IP address.
	// unsigned char __ar_tha[ETH_ALEN];        // Target hardware address.
	// unsigned char __ar_tip[4];                // Target IP address.
	
	// Set the sender hardware (MAC) address. That's ours.
	memcpy(payload, &us->sll_addr, arpHeader->ar_hln);
	payload += us->sll_halen;

	// Set the sender IPv4 address.
	memcpy(payload, senderIp, 4);
	payload += 4;

	// Set the target hardware (MAC) address.
	memcpy(payload, them->sll_addr, arpHeader->ar_hln);
	payload += arpHeader->ar_hln;

	// Set the target IPv4 address.
	memcpy(payload, destinationIp, 4);
	payload += 4;

	ssize_t bytesSent = sendto(socketFd, backingBuffer, payload - backingBuffer, 0, (struct sockaddr *) them, sll_len(arpHeader->ar_hln));
	if (bytesSent != (payload - backingBuffer)) {
		fprintf(stderr, "Failed to send ARP request\n");
		exit(1);
	}
}

static void findDeviceInfo(const char *device, struct ifaddrs **out_info) {
	struct ifaddrs *devices = NULL;
	if (0 != getifaddrs(&devices)) {
		fprintf(stderr, "Failed to get network device information\n");
		exit(1);
	}

	struct ifaddrs *ifa;
	for  (ifa = devices; ifa; ifa = ifa->ifa_next) {
		// We want to find the given device but the instance of it that supports the PF_PACKET family, so that we can connect to it with raw sockets.
		// We then also want to make sure the device is up, it is arpable, and that it is not the loopback (where ARP doesn't make any sense).
		if ((0 == strcmp(device, ifa->ifa_name)) && (ifa->ifa_addr->sa_family == AF_PACKET)) {
			if (0 == (ifa->ifa_flags & IFF_UP)) {
				fprintf(stderr, "Failed - device %s is not UP\n", device);
				exit(1);
			}
			if (0 != (ifa->ifa_flags & IFF_LOOPBACK)) {
				fprintf(stderr, "Failed - device %s is a loopback device\n", device);
				exit(1);
			}
			if (0 != (ifa->ifa_flags & IFF_NOARP)) {
				fprintf(stderr, "Failed - device %s does not support ARP\n", device);
				exit(1);
			}

			*out_info = ifa;
			return;
		}
	}

	fprintf(stderr, "Failed to find device information for device: %s\n", device);
	exit(1);
}

int main(int argc, const char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Incorrct number of args\n");
		fprintf(stderr, "USAGE: %s <device> <ip>\n", argv[0]);
		exit(1);
	}

	const char *device = argv[1];
	const char *ip = argv[2];

	struct ifaddrs *deviceInfo = NULL;
	findDeviceInfo(device, &deviceInfo);
	assert(NULL != deviceInfo);

	int socketFd = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (-1 == socketFd) {
		fprintf(stderr, "Failed to create raw socket\n");
		exit(1);
	}

	struct in_addr address;
	if (1 != inet_aton(ip, &address)) {
		fprintf(stderr, "Invalid IPv4 address given: %s\n", ip);
		exit(1);
	}

	unsigned int deviceIndex = if_nametoindex(device);
	if (0 == deviceIndex) {
		fprintf(stderr, "Failed to get index of given device: %s\n", device);
		exit(1);
	}

	// Create a probe. We use the probe to probe the device for certain information we use to connect our real socket to.
	int probeFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == probeFd) {
		fprintf(stderr, "Failed to create probe socket\n");
		exit(1);
	}

	// Bind our probeFd to the given device.
	if (0 != setsockopt(probeFd, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device) + 1)) {
		fprintf(stderr, "Failed to bind probe to device: %s\n", device);
		exit(1);
	}

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	socklen_t alen = sizeof(saddr);

	saddr.sin_port = htons(1025);
	saddr.sin_addr = address;

	// Don't allow our packets to be routed. ARP is only for a local ethernet connection.
	int dontRoute = 1;
	if (0 != setsockopt(probeFd, SOL_SOCKET, SO_DONTROUTE, (char *) &dontRoute, sizeof(dontRoute))) {
		fprintf(stderr, "Failed to set probe as dont route\n");
		exit(1);
	}

	if (0 != connect(probeFd, (struct sockaddr *) &saddr, sizeof(saddr))) {
		fprintf(stderr, "Failed to connect probe\n");
		exit(1);
	}

	// We bound to the device, now get the IPv4 address of that device.
	if (0 != getsockname(probeFd, (struct sockaddr *) &saddr, &alen)) {
		fprintf(stderr, "Failed to get ip of device: %s\n", device);
		exit(1);
	}

	// Close the probe. It seems the only reason we did this was so we could take advantage of SO_BINDTODEVICE to bind to a device, since AF_INET can use that but not PF_PACKET.
	assert(0 == close(probeFd));

	// Bind our socket now as a packet family, using the ARP protocol to the device index.
	struct sockaddr_storage me;
	((struct sockaddr_ll *) &me)->sll_family = AF_PACKET;
	((struct sockaddr_ll *) &me)->sll_ifindex = deviceIndex;
	((struct sockaddr_ll *) &me)->sll_protocol = htons(ETH_P_ARP);
	if (0 != bind(socketFd, (struct sockaddr *) &me, sizeof(me))) {
		fprintf(stderr, "Failed to bind socket to device\n");
		exit(1);
	}

	// Get the socket name information back and then make sure it has a hardware address so we can ARP it (sll_halen is the ha (hardware) len (length) of the address).
	socklen_t len = sizeof(me);
	if (0 != getsockname(socketFd, (struct sockaddr *) &me, &len)) {
		fprintf(stderr, "Failed to get socket name\n");
		exit(1);
	}
	if (0 == ((struct sockaddr_ll *) &me)->sll_halen) {
		fprintf(stderr, "Device %s is not ARPable (no hardware address)\n", device);
		exit(1);
	}

	// Set the other's address to our broadcast address.
	struct sockaddr_storage other;
	memcpy(&other, &me, sizeof(struct sockaddr_storage));

	struct sockaddr_ll *sll = (struct sockaddr_ll *) deviceInfo->ifa_broadaddr;
	memcpy(((struct sockaddr_ll *) &other)->sll_addr, sll->sll_addr, ((struct sockaddr_ll *) &other)->sll_halen);

	printf("ARPING %s\n", inet_ntoa(address));
	printf("from %s %s\n", inet_ntoa(saddr.sin_addr), device);

	sendArpPacket(socketFd, (struct sockaddr_ll *) &me, (struct sockaddr_ll *) &other, &saddr.sin_addr, &address);

	bool receivedReply = false;
	while (!receivedReply) {
		receivedReply = recvArpPacket(socketFd, (struct sockaddr_ll *) &me, &saddr.sin_addr, &address);
	}
	
	return 0;
}
