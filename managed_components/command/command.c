#include "command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ping.h>
#include <lownet_util.h>

#include <mbedtls/sha256.h>
#include <mbedtls/rsa.h>
#include <mbedtls/pk.h>

#include "signature.h"

typedef enum
{
	LISTENING,
	WAIT_SIG,
	WAIT_SIG1,
	WAIT_SIG2,
} state_t;

typedef enum
{
	UNSIGNED = 0b00,
	SIGNED = 0b01,
	SIG1 = 0b10,
	SIG2 = 0b11,
} frame_type_t;

typedef enum
{
	TIME = 0x01,
	TEST = 0x02,
} command_type_t;

typedef struct
{
	mbedtls_pk_context key;
	hash_t hash;
} public_key_t;

static struct {
	state_t state;
	uint64_t last_valid;
	lownet_time_t command_received;
	lownet_frame_t current_cmd;
	hash_t hash;
	signature_t signature;
	public_key_t key;
} state;

// Usage: get_frame_type(FRAME)
// Pre:   FRAME is a valid lownet frame
// Value: The type of the frame
frame_type_t get_frame_type(const lownet_frame_t* frame)
{
	return (frame->protocol & 0b11000000) >> 6;
}

// Usage: command_ready_next()
// Pre:   Any command frame currently being processed has either
//        completed processing or been discarded
// Post: The command module is ready to receive another command packet
void command_ready_next()
{
	state.state = LISTENING;
	memset(&state.current_cmd, 0, sizeof(lownet_frame_t));
	memset(&state.command_received, 0, sizeof(lownet_time_t));
	memset(&state.hash, 0, sizeof(hash_t));
	memset(&state.signature, 0, sizeof(signature_t));
}

// Usage: public_key_init(PEM, KEY)
// Pre:   PEM is a pem encoded public key
//        KEY != NULL
// Post:  KEY has been initialised
// Value: 0 if KEY was successfully initialised, non-0 otherwise
int public_key_init(const char* pem, public_key_t* key)
{
	mbedtls_pk_init(&key->key);
	size_t length = strlen(pem);

	int status = mbedtls_pk_parse_public_key(
		&key->key,
		(const unsigned char*) pem,
		length + 1
	);
	if (status)
		return status;

	status = hash(pem, length, &key->hash);
	return status;
}

// Usage: verify_signature(KEY)
// Pre:   A full signature has been received
//        KEY has been initialised by public_key_init
// Value: true if the received signature matches the current command
bool verify_signature(const public_key_t* key)
{
	signature_t signature;
	mbedtls_rsa_public(
		mbedtls_pk_rsa(key->key),
		state.signature.bytes,
		signature.bytes
	);

	signature_t expected;
	memset(expected.bytes, 0, 220);
	memset(expected.bytes + 220, 1, 4);
	memcpy(expected.bytes + 220 + 4, state.hash.bytes, sizeof(hash_t));

	return signature_equal(&signature, &expected);
}

// Usage: command_time_cmd(TIME)
// Pre:   None
// Post:  System time has been set to TIME
void command_time_cmd(const lownet_time_t* time)
{
	lownet_set_time(time);
}

// Usage: command_test_cmd(FRAME)
// Pre:   FRAME's protocol is the command protocol, the command
//        contained in FRAME is of type Test
// Post: A ping has been sent to FRAME's source, as per the Test
//       command specification
void command_test_cmd(const lownet_frame_t* frame)
{
	const cmd_packet_t* command = (const cmd_packet_t*) &frame->payload;
	ping(frame->source, command->contents, frame->length - CMD_HEADER_SIZE);
}

// Usage: command_execute(COMMAND)
// Pre:   The signature of the COMMAND has been verified
// Post:  The COMMAND has been executed
void command_execute(const lownet_frame_t* frame)
{
	const cmd_packet_t* command = (const cmd_packet_t*) &frame->payload;
	switch (command->type)
		{
		case TIME:
			command_time_cmd((const lownet_time_t*) command->contents);
			return;
		case TEST:
			command_test_cmd(frame);
			return;
		}
}

// Usage: signature_received()
// Pre:   A full signature for the current command frame has been
//        received
// Post: The signature of the current command frame has been verified
//       and if correct the command has been executed
void signature_received()
{
	if (!verify_signature(&state.key))
		{
			// Invalid signature, discard the command.
			command_ready_next();
			return;
		}

	state.last_valid = ((const cmd_packet_t*) state.current_cmd.payload)->sequence;
	command_execute(&state.current_cmd);
}

void handle_command_frame(const lownet_frame_t* frame)
{
	const cmd_packet_t* command = (const cmd_packet_t*) &frame->payload;
	if (command->sequence < state.last_valid)
		return;

	// Discard command in processing to handle this new one.
	// TODO: Allow multiple commands at the same time.
	command_ready_next();

	lownet_time_t now = lownet_get_time();
	memcpy(&state.command_received, &now, sizeof(lownet_time_t));
	if (hash((const char*) frame, sizeof *frame, &state.hash))
		{
			// Something went wrong hashing the frame, discard it.
			command_ready_next();
			return;
		}

	memcpy(&state.current_cmd, frame, sizeof *frame);
	state.state = WAIT_SIG;
}

void handle_signature_part1(const cmd_signature_t* signature)
{
	memcpy(state.signature.bytes, signature->sig_part, sizeof signature->sig_part);
	if (state.state == WAIT_SIG)
		{
			state.state = WAIT_SIG2;
			return;
		}

	if (state.state == WAIT_SIG1)
		signature_received();
}

void handle_signature_part2(const cmd_signature_t* signature)
{
	memcpy(state.signature.bytes + (sizeof(signature_t) / 2),
		signature->sig_part,
		sizeof signature->sig_part
	);
	if (state.state == WAIT_SIG)
		{
			state.state = WAIT_SIG1;
			return;
		}

	if (state.state == WAIT_SIG2)
		signature_received();
}

// Usage: handle_signature_frame(FRAME)
// Pre:   get_frame_type(FRAME) = SIG1 or SIG2
// Post:  FRAME has been processed
void handle_signature_frame(const lownet_frame_t* frame)
{
	const static lownet_time_t timeout = {10, 0};
	frame_type_t type = get_frame_type(frame);
	const cmd_signature_t* signature = (const cmd_signature_t*) &frame->payload;

	// If the msg hash does not match the current command this is a
	// signature for a different command.  Discard it.
	if (!hash_equal(&signature->hash_msg, &state.hash))
		return;

	lownet_time_t now = lownet_get_time();
	lownet_time_t diff = time_diff(&state.command_received, &now);
	if (compare_time(&diff, &timeout) > 0)
		{
			// Signature took to long to arrive, discard the command
			command_ready_next();
			return;
		}

	if (!hash_equal(&state.key.hash, &signature->hash_key))
			return;

	switch (type)
		{
		case SIG1:
			handle_signature_part1(signature);
			return;
		case SIG2:
			handle_signature_part2(signature);
		default:
			return;
		}
}

void command_init()
{
	command_ready_next();
	state.last_valid = 0;

	public_key_init(lownet_get_signing_key(), &state.key);
}

void command_receive(const lownet_frame_t* frame)
{
	frame_type_t type = get_frame_type(frame);
	switch (type)
		{
		case UNSIGNED:
			return;

		case SIGNED:
			handle_command_frame(frame);
			return;

		case SIG1:
		case SIG2:
			handle_signature_frame(frame);
			return;
		}
}
