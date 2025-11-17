#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_log.h>

#include <string.h>

#include <lownet.h>
#include <serial_io.h>
#include <utility.h>
#include <lownet_util.h>

#include "chat.h"

#define TAG "CHAT"

void chat_init()
{
	if (lownet_register_protocol(LOWNET_PROTOCOL_CHAT, chat_receive) != 0)
		{
			ESP_LOGE(TAG, "Error registering CHAT protocol");
		}
}

void shout_command(char* args)
{
	chat_shout(args);
}

void tell_command(char* args)
{
	char* dest = strtok(args, " ");
	char* message = strtok(NULL, "\n");

	uint8_t d = (uint8_t) hex_to_dec(dest + 2);
	if (d == 0)
		{
			serial_write_line("Invalid node id\n");
			return;
		}

	if (!message)
		{
			serial_write_line("A message must be provided\n");
			return;
		}

	chat_tell(message, d);
}

void chat_receive(const lownet_frame_t* frame) {
	if (frame->destination != lownet_get_device_id()
			&& frame->destination != LOWNET_BROADCAST_ADDRESS)
		return;

	char msg[frame->length + 1];
	memcpy(msg, &frame->payload, frame->length);
	msg[frame->length] = '\0';

	// id + max(len("shouts"), len("says")) + message (+ addre... + id) + null
	char buffer[4 + 9 + frame->length + 16 + 4 + 1 + 1];
	int n = 0;
	n += format_id(buffer + n, frame->source);
	n += sprintf(buffer + n, " %s: ", (frame->destination != LOWNET_BROADCAST_ADDRESS) ? "says" : "shouts");
	n += sprintf(buffer + n, "%s", msg);

	serial_write_line(buffer);
}

// Usage: chat_valid_message(MESSAGE)
// Pre:   MESSAGE != NULL
// Value: strlen(MESSAGE) if MESSAGE is a valid message, 0 otherwise
size_t chat_valid_message(const char* message)
{
	size_t i = 0;
	/*
		Loop Invariant:
		0 <= i <= strlen(message)
		forall x | 0 <= x < i : util_printable(message[x])
	 */
	for (; message[i]; i++)
		{
			if (!util_printable(message[i]))
				return 0;
		}

	if (i > LOWNET_PAYLOAD_SIZE)
		return 0;


	return i;
}

void chat_shout(const char* message) {
	chat_tell(message, LOWNET_BROADCAST_ADDRESS);
}

void chat_tell(const char* message, uint8_t destination) {
	size_t length = 0;
	if (!(message && (length = chat_valid_message(message))))
		return;

	lownet_frame_t frame;
	frame.source = lownet_get_device_id();
	frame.destination = destination;
	frame.protocol = LOWNET_PROTOCOL_CHAT;
	frame.length = length;
	memcpy(&frame.payload, message, length);
	lownet_send(&frame);
}
