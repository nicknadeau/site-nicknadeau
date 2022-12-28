#include "packet_listener.h"

#include <stdio.h>
#include <stdlib.h>
#include <net/if_arp.h>
#include <assert.h>

static uint16_t MAC_ADDRESS_TYPE = 0x0001;
static uint16_t IPV4_ADDRESS_TYPE = 0x0800;

static void printMacAddress(const unsigned char *address) {
	for (int i = 0; i < 6; i++) {
		printf("%02x", address[i]);
		if (i < 5) {
			printf(":");
		}
	}
}

static void printIpv4Address(const unsigned char *address) {
	for (int i = 0; i < 4; i++) {
		printf("%u", address[i]);
		if (i < 3) {
			printf(".");
		}
	}
}

static void packetHandler(arp_datagram_t *datagram) {
	if ((datagram->hardwareAddressType == MAC_ADDRESS_TYPE) && (datagram->protocolAddressType == IPV4_ADDRESS_TYPE)) {
		assert(datagram->hardwareAddressSize == 6);
		assert(datagram->protocolAddressSize == 4);

		if (datagram->arpOpcode == ARPOP_REPLY) {
			printf("ARP Reply arrived!\n");
			printf("Sender MAC address: ");
			printMacAddress(datagram->senderHardwareAddress);
			printf("\nSender IPv4 address: ");
			printIpv4Address(datagram->senderProtocolAddress);
			printf("\nDestination MAC address: ");
			printMacAddress(datagram->destinationHardwareAddress);
			printf("\nDestination IPv4 address: ");
			printIpv4Address(datagram->destinationProtocolAddress);
			printf("\n\n");

			fflush(stdout);
		}
	}
}

int main(int argc, const char **argv) {
	startPacketListener(packetHandler);
	return 0;
}
