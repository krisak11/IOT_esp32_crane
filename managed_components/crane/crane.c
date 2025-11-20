#include "crane.h"

#include <string.h>

#include <esp_log.h>

#include <lownet.h>
#include <utility.h>
#include <serial_io.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <stdbool.h>


#define CRANE_PROTO 0x05

#define TAG "crane"

void crane_connect(uint8_t id);
void crane_disconnect();
int  crane_action(uint8_t action); // returns zero if ACK is received
void crane_test(uint8_t id);
void crane_receive(const lownet_frame_t* frame);
void crane_send(uint8_t destination, const crane_packet_t* packet);

// state of a single flow
static struct
{
	uint16_t seq; // next sequence number to use
	uint8_t crane; 
	uint8_t  backlog; // latest backlog from STATUS
	QueueHandle_t acks;
	enum
		{
			ST_DISCONNECTED,
			ST_HANDSHAKE,
			ST_CONNECTED,
		} state;
	bool test_mode; // whether in test mode or normal mode
} state;

int crane_init(void)
{
	if (lownet_register_protocol(CRANE_PROTO, crane_receive) != 0)
		{
			ESP_LOGE(TAG, "Failed to register crane protocol");
			return 1;
		}

	state.seq = 0;
	state.crane = 0;
	state.backlog = 0; // keep track of backlog
	state.state = ST_DISCONNECTED;
	state.test_mode = false; // not in test mode by default
	state.acks = xQueueCreate(8, sizeof(uint16_t));
	ESP_LOGI(TAG, "Crane protocol initialized");

	return 0;
}

void crane_command(char* args)
{
	if (!args)
		{
			serial_write_line("Missing argument COMMAND");
			return;
		}
	char* saveptr;

	char* command = strtok_r(args, " ", &saveptr);
	if (!command)
		{
			serial_write_line("Missing argument COMMAND");
			return;
		}

	if (strcmp(command, "help") == 0)
		{
			serial_write_line("open ID    Connect to a crane at ID");
			serial_write_line("close      Close an existing connection");
			serial_write_line("test ID    Connect to ID in test mode and execute test pattern");
			serial_write_line("CMD        Implementation defined commands to trigger crane actions");
		}
	else if (strcmp(command, "open") == 0)
		{
			char* id = strtok_r(NULL, " ", &saveptr);
			if (!id)
				{
					serial_write_line("Missing argument ID");
					return;
				}
			uint8_t dest = hex_to_dec(id + 2);
			crane_connect(dest);
		}
	else if (strcmp(command, "close") == 0)
		{
			crane_disconnect();
		}
	else if (strcmp(command, "test") == 0)
		{
			char* id = strtok_r(NULL, " ", &saveptr);
			if (!id)
				{
					serial_write_line("Missing argument ID");
					return;
				}
			uint8_t dest = hex_to_dec(id + 2);
			crane_test(dest);
		}
	else
		{
			uint8_t action = CRANE_NULL;
			switch (command[0])
				{
					// ------------------------------------------------
					// Milestone II, Task 2: implement commands to CLI
				    case 'f':  // FORWARD
						action = CRANE_FWD;
						break;

					case 'b':  // BACKWARD
						action = CRANE_REV;
						break;

					case 'u':  // UP
						action = CRANE_UP;
						break;

					case 'd':  // DOWN
						action = CRANE_DOWN;
						break;

					case 'o':  // light on
						action = CRANE_LIGHT_ON;
						break;

					case 'O':  // light off
						action = CRANE_LIGHT_OFF;
						break;
				// ------------------------------------------------
				default:
					ESP_LOGI(TAG, "Invalid crane command");
					return;
				}
			crane_action(action);
		}
}

void crane_recv_connect(const crane_packet_t* packet)
{
	ESP_LOGI(TAG, "Received CONNECT packet");
	if (state.state != ST_HANDSHAKE)
		return;

	ESP_LOGI(TAG, "packet flags: %02x", packet->flags);

	// ------------------------------------------------
	// Milestone I: Check SYN and ACK flags/bits are both set (SYN-ACK) - if not, abort handshake.
    if (((packet->flags & CRANE_SYN) == 0) ||
        ((packet->flags & CRANE_ACK) == 0))
    {
        ESP_LOGW(TAG, "CONNECT packet without SYN|ACK, aborting handshake");
        return;
    }

	// Milestone I: respond with appropriate (ack) packet + challenge! (cf. Task 2)
	// Build final ACK with inverted challenge ("proof of work")
	crane_packet_t outpkt = {0};
	outpkt.type  = CRANE_CONNECT;
	outpkt.flags = CRANE_ACK;           // add CRANE_TEST here if doing test mode
	outpkt.seq   = 0;      				// handshake uses seq = 0

	// flip all bits of the challenge
    outpkt.d.conn.challenge = ~packet->d.conn.challenge;

    crane_send(state.crane, &outpkt);

    // handshake complete
    state.state = ST_CONNECTED;
    // After handshake, actions start at seq = 1; crane_connect() already did ++seq
    ESP_LOGI(TAG, "Handshake completed, connection established");
}
	// ------------------------------------------------

void crane_recv_close(const crane_packet_t* packet)
{
	ESP_LOGI(TAG, "Closing connection");
	state.seq = 0;
	state.state = ST_DISCONNECTED;
	state.crane = 0;
}

void crane_recv_status(const crane_packet_t* packet)
{
    char buffer[200];

    ESP_LOGI(TAG, "STATUS: seq=%u flags=0x%02x", packet->seq, packet->flags);

    if (packet->flags & CRANE_NAK)
        {
            ESP_LOGI(TAG, "Received status packet with NAK -- not in use yet");
            return;
        }
    else
        xQueueSend(state.acks, &packet->seq, 0);

    snprintf(buffer, sizeof buffer,
             "backlog: %d\n"
             "time: %d\n"
             "light: %s\n",
             packet->d.status.backlog, // 
             packet->d.status.time_left,
             packet->d.status.light ? "on" : "off");
    serial_write_line(buffer);
}


void crane_receive(const lownet_frame_t* frame)
{
	crane_packet_t packet;
	memcpy(&packet, frame->payload, sizeof packet);
	ESP_LOGI(TAG, "Received packet frame from %02x, type: %d", frame->source, packet.type);
	switch (packet.type)
		{
		case CRANE_CONNECT:
			crane_recv_connect(&packet);
			break;
		case CRANE_STATUS:
			crane_recv_status(&packet);
			break;
		case CRANE_ACTION:
			break;
		case CRANE_CLOSE:
			crane_recv_close(&packet);
		}
}

/*
 * This function starts the connection establishment
 * procedure by sending a SYN packet to the given node.
 */
void crane_connect(uint8_t id)
{
    if (state.state != ST_DISCONNECTED)
        return;

    crane_packet_t packet = {0};      // zero-initialize everything

    packet.type  = CRANE_CONNECT;
    packet.flags = CRANE_SYN;         // just SYN for normal open
    packet.seq   = 0;                 // handshake always uses seq = 0
    packet.d.conn.challenge = 0;      // MUST be 0 in the initial request

    // Internal state
    state.crane = id;
    state.state = ST_HANDSHAKE;
    state.seq = 1;           // Commands must be numbered using the seq-field starting with 1 after connection establishment.       

    ESP_LOGI(TAG, "Sending CONNECT (SYN) to 0x%02x", id);
    crane_send(id, &packet);
}


void crane_disconnect(void)
{
    if (state.state == ST_DISCONNECTED){
		ESP_LOGI(TAG, "state already disconnected");
        return;
	}
		

    crane_packet_t packet = {0};

    packet.type  = CRANE_CLOSE;
    packet.flags = 0;              // request close, no ACK yet
    packet.seq   = state.seq;      // next sequence number
    packet.d.close = 0;            // reserved

	ESP_LOGI(TAG, "sending CRANE_ClOSE to 0x%02x", state.crane);
    crane_send(state.crane, &packet);

    // TODO (optional): wait for CLOSE+ACK and retransmit up to 3 times

    // Then we update our state
    state.state = ST_DISCONNECTED;
    state.seq   = 0;
    state.crane = 0;

}

/*
 *	Subroutine for crane_action: read ACKs from crane, blocks for some time
 */
uint16_t read_acks(void)
{
	uint16_t seq, x;

	// Wait for an ack up to 5 seconds
	if ( xQueueReceive(state.acks, &seq, 5000/portTICK_PERIOD_MS) != pdTRUE ){
		ESP_LOGW(TAG, "No ACK received within timeout, seq set to 0. state.acks value is %p", state.acks);
		seq = 0;
	}
		
	// read any other acks if in the queue
	while ( xQueueReceive(state.acks, &x, 0) == pdTRUE ){
		seq = seq >= x ? seq : x;
		ESP_LOGW(TAG, "Drained ACK from queue, seq now %u", seq);
	}
	ESP_LOGW(TAG, "Returning seq=%u from read_acks", seq);
	return seq;
}

/*
 *	This can block for a while if no immediate ACK
 */
int crane_action(uint8_t action)
{
    crane_packet_t packet = {0};      // <-- important

    packet.type  = CRANE_ACTION;
    packet.flags = 0;                 // normal action: no SYN/ACK/NAK/TEST
    packet.seq   = state.seq; 		// current sequence number
    ESP_LOGW(TAG, "crane_action: sending action %u with seq=%u", action, packet.seq);
	packet.d.action.cmd = action;
    // reserved already zeroed by = {0}

    // First send
    crane_send(state.crane, &packet);
	uint16_t seq = read_acks();
	ESP_LOGI(TAG, "read_acks: got seq=%u, state.seq=%u", seq, state.seq);
    // ------------------------------------------------
    // Milestone II, Task 1: up to five attempts
    for (int attempt = 0; attempt < 5; ++attempt)
        {
            uint16_t seq = read_acks(); // cumulative ACK, or 0 if none
            ESP_LOGI(TAG, "read_acks: got seq=%u, state.seq=%u", seq, state.seq);

            if (seq > state.seq)
                {
                    ESP_LOGE(TAG, "Received future ACK (seq=%u > state.seq=%u), closing",
                             seq, state.seq);
                    crane_disconnect();
                    return -2;
                }
            else if (seq == state.seq)
                {
                    // Our action was acknowledged
                    state.seq++;       // next action gets next seq
                    return 0;
                }

            // seq < state.seq or 0 → no ACK for this command yet, retransmit
            ESP_LOGW(TAG, "No valid ACK yet for state.seq=%u (attempt %d), retransmitting",
                     state.seq, attempt + 1);
            crane_send(state.crane, &packet);
        }
    // ------------------------------------------------

    ESP_LOGI(TAG, "Received no ack from node=0x%02x disconnecting from crane.", state.crane);
    crane_disconnect();
    return -1;
}




// ------------------------------------------------
//
// Milestone III: run the test pattern
//
// 1. establish connection with TEST flag
// 2. run the test pattern according to the specs:
//		Resend command(s) accordingly if no reception ACK is received within 1-2 seconds.
// 		2.1. Establish a connection to crane
// 		2.2. Switch on the light
// 		2.3. Drive crane forward for two steps
// 		2.4. Drive crane backward for one step
// 		2.5. Pause until all commands have executed. Wait for an ACK that confirms completion of all commands
// 		2.6. Lower the hook for two steps
// 		2.7. Pause until all commands have executed. Wait for an ACK that confirms completion of all commands
// 		2.8. Lift the hook back up for two steps
// 		2.9. Drive crane backwards for one step
// 		2.10. Switch of the light
// 		2.11. Disconnect by sending the CLOSE command
// 3. close the connection
//
// Hint: you can work it all out here slowly, or be a wizard
//       and launch a separate task for this!
//
void crane_test(uint8_t id)
{
}
// ------------------------------------------------


void crane_send(uint8_t id, const crane_packet_t* packet)
{
	lownet_frame_t frame;
	frame.destination = id;
	frame.protocol = CRANE_PROTO;
	frame.length = sizeof *packet;
	memcpy(frame.payload, packet, sizeof *packet);
	// Debug: print out critical fields
    ESP_LOGI(TAG,
             "crane_send: dst=0x%02x proto=0x%02x len=%d | "
             "type=%u flags=0x%02x seq=%u",
             frame.destination,
             frame.protocol,
             frame.length,
             packet->type,
             packet->flags,
             packet->seq);

	lownet_send(&frame);
}
