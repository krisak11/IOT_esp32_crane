#ifndef COMMAND_H
#define COMMAND_H

#include <stdint.h>
#include <assert.h>

#include <lownet.h>

#include "hash.h"

#define LOWNET_PROTOCOL_COMMAND 0x04

#define CMD_HASH_SIZE 32
#define CMD_BLOCK_SIZE 256

#define CMD_HEADER_SIZE 12
#define CMD_PAYLOAD_SIZE LOWNET_PAYLOAD_SIZE - CMD_HEADER_SIZE

typedef struct __attribute__((__packed__))
{
	uint64_t sequence;
	uint8_t type;
	uint8_t reserved[3];
	uint8_t contents[CMD_PAYLOAD_SIZE];
} cmd_packet_t;

static_assert(CMD_PAYLOAD_SIZE == 188);
static_assert(sizeof(cmd_packet_t) == CMD_HEADER_SIZE + CMD_PAYLOAD_SIZE);

typedef struct __attribute__((__packed__))
{
	hash_t hash_key;
	hash_t hash_msg;
	uint8_t sig_part[CMD_BLOCK_SIZE / 2];
} cmd_signature_t;

void command_init();
void command_receive(const lownet_frame_t* frame);
#endif
