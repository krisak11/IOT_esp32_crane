#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ping.h"

#include "serial_io.h"

typedef struct __attribute__((__packed__))
{
	lownet_time_t timestamp_out;
	lownet_time_t timestamp_back;
	uint8_t origin;
} ping_packet_t;


void ping(uint8_t node) {
	lownet_frame_t frame;
	memset(&frame, 0, sizeof(frame));

	frame.source = lownet_get_device_id();
	frame.destination = node;
	frame.protocol = LOWNET_PROTOCOL_PING;
	frame.length = sizeof(ping_packet_t);

	ping_packet_t pak;
	memset(&pak, 0, sizeof(pak));
	pak.timestamp_out = lownet_get_time();
	pak.origin = lownet_get_device_id();
	memcpy(frame.payload, &pak, sizeof(pak));

	lownet_send(&frame);

	char buffer[MSG_BUFFER_LENGTH];
	memset(buffer, 0, MSG_BUFFER_LENGTH);
	sprintf(buffer, "<PING OUT : 0x%02X>", node);
	serial_write_line(buffer);
}

void ping_receive(const lownet_frame_t* frame) {
	if (frame->length < sizeof(ping_packet_t)) {
		// Malformed frame.  Discard.
		return;
	}

	ping_packet_t pak;
	memcpy(&pak, frame->payload, sizeof(pak));

	if (pak.origin == lownet_get_device_id()) {
		// This is a response to one of our pings.  Print a diagnostic.
		char buffer[MSG_BUFFER_LENGTH];
		memset(buffer, 0, MSG_BUFFER_LENGTH);
		sprintf(buffer, "<PING BACK : 0x%02X>", frame->source);
		serial_write_line(buffer);
	} else {
		// This is a ping request from elsewhere.  Respond.
		lownet_frame_t out_frame;
		memset(&out_frame, 0, sizeof(out_frame));

		out_frame.source = lownet_get_device_id();
		out_frame.destination = frame->source;
		out_frame.protocol = LOWNET_PROTOCOL_PING;
		out_frame.length = frame->length;

		pak.timestamp_back = lownet_get_time();
		memcpy(out_frame.payload, &pak, sizeof(pak));
		if (frame->length > sizeof(ping_packet_t)) {
			memcpy(
				out_frame.payload + sizeof(ping_packet_t),
				frame->payload + sizeof(ping_packet_t),
				(frame->length - sizeof(ping_packet_t))
			);
		}

		lownet_send(&out_frame);
	}
}
