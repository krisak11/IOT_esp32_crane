#ifndef GUARD_APP_PING_H
#define GUARD_APP_PING_H

#include <stdint.h>

#include "lownet.h"

#define LOWNET_PROTOCOL_PING 0x03

void ping_init();

// Usage: ping_command(ID)
// Pre:   ID is a valid node id.
// Post:  A ping has been sent to the node identified by ID.
void ping_command(char* args);

// Usage: ping(NODE, PAYLOAD, LENGTH)
// Pre:   NODE is a node id, PAYLOAD is NULL or a pointer to a buffer of
//        size LENGTH,
//        LENGTH <= LOWNET_PAYLOAD_SIZE - sizeof(ping_packet_t)
// Post: A ping has been sent to the node identified by NODE
//       Any data contained in a non-NULL PAYLOAD have been included
//       in the ping message.
void ping(uint8_t node, const uint8_t* payload, uint8_t length);

void ping_receive(const lownet_frame_t* frame);

typedef struct __attribute__((__packed__))
{
	lownet_time_t timestamp_out;
	lownet_time_t timestamp_back;
	uint8_t origin;
} ping_packet_t;

#endif
