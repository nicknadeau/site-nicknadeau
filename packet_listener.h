#ifndef __PACKET_LISTENER_H__
#define __PACKET_LISTENER_H__

#include <stdint.h>

#define MAX_ADDRESS_LEN 256

typedef struct arp_datagram {
	uint16_t hardwareAddressType;
	uint16_t protocolAddressType;
	uint8_t hardwareAddressSize;
	uint8_t protocolAddressSize;
	uint16_t arpOpcode;
	unsigned char senderHardwareAddress[MAX_ADDRESS_LEN];
	unsigned char senderProtocolAddress[MAX_ADDRESS_LEN];
	unsigned char destinationHardwareAddress[MAX_ADDRESS_LEN];
	unsigned char destinationProtocolAddress[MAX_ADDRESS_LEN];
} arp_datagram_t;

typedef void (*packet_handler_t)(arp_datagram_t *datagram);

void startPacketListener(packet_handler_t handler);

#endif	//__PACKET_LISTENER_H__
