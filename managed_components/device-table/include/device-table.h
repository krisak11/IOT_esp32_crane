#ifndef DEVICE_TABLE_H
#define DEVICE_TABLE_H

#include <stdint.h>

typedef struct {
	uint8_t mac[6];
	uint8_t node;
} lownet_identifier_t;

lownet_identifier_t lownet_lookup(uint8_t id);
lownet_identifier_t lownet_lookup_mac(const uint8_t* mac);

#endif
