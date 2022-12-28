#include "packet_listener.h"

#include <sys/socket.h>
#include <assert.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <net/ethernet.h>
#include <string.h>


void startPacketListener(packet_handler_t handler) {
	int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	assert(-1 != fd);

	char buffer[4096];
	int bytesRead = read(fd, buffer, sizeof(buffer));
	while (bytesRead > 0) {
		uint16_t *protocolNetByteOrder = (uint16_t *) (buffer + 12);
		uint16_t protocolType = ntohs(*protocolNetByteOrder);

		if (ETHERTYPE_ARP == protocolType) {
			assert(bytesRead >= 22);

			uint16_t *hardwareType = (uint16_t *) (buffer + 14);
			uint16_t *protocolType = hardwareType + 1;
			uint8_t *hardwareSize = (uint8_t *) (buffer + 18);
			uint8_t *protocolSize = hardwareSize + 1;
			uint16_t *opcode = protocolType + 2;

			arp_datagram_t datagram = {
				.hardwareAddressType = ntohs(*hardwareType),
				.protocolAddressType = ntohs(*protocolType),
				.hardwareAddressSize = *hardwareSize,
				.protocolAddressSize = *protocolSize,
				.arpOpcode = ntohs(*opcode),
			};

			assert(bytesRead >= 22 + (2 * datagram.hardwareAddressSize) + (2 * datagram.protocolAddressSize));

			memcpy(&datagram.senderHardwareAddress, buffer + 22, datagram.hardwareAddressSize);
			memcpy(&datagram.senderProtocolAddress, buffer + 22 + datagram.hardwareAddressSize, datagram.protocolAddressSize);
			memcpy(&datagram.destinationHardwareAddress, buffer + 22 + datagram.hardwareAddressSize + datagram.protocolAddressSize, datagram.hardwareAddressSize);
			memcpy(&datagram.destinationProtocolAddress, buffer + 22 + (2 * datagram.hardwareAddressSize) + datagram.protocolAddressSize, datagram.protocolAddressSize);

			handler(&datagram);
		}

		bytesRead = read(fd, buffer, sizeof(buffer));
	}
}
