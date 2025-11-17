#ifndef GUARD_LOWNET_H
#define GUARD_LOWNET_H

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define LOWNET_SERVICE_CORE 1
#define LOWNET_SERVICE_PRIO 10

#define LOWNET_MAX_PROTOCOLS 10

#define LOWNET_FRAME_SIZE 212
#define LOWNET_HEAD_SIZE 8
#define LOWNET_CRC_SIZE 4
#define LOWNET_PAYLOAD_SIZE (LOWNET_FRAME_SIZE - (LOWNET_HEAD_SIZE + LOWNET_CRC_SIZE)) // 200 bytes.
#define LOWNET_IVT_SIZE 16
#define LOWNET_UNENCRYPTED_SIZE 4
#define LOWNET_ENCRYPTED_SIZE 208

#define LOWNET_KEY_SIZE_AES 32
#define LOWNET_KEY_SIZE_RSA 256

#define LOWNET_BROADCAST_ADDRESS 0xFF
#define LOWNET_TIME_RESOLUTION 256

typedef struct __attribute__((__packed__))
{
	uint8_t magic[2];
	uint8_t source;
	uint8_t destination;
	uint8_t protocol;
	uint8_t length;
	uint8_t padding[2];
	uint8_t payload[LOWNET_PAYLOAD_SIZE];
	uint32_t crc;
} lownet_frame_t;

typedef struct __attribute__((__packed__))
{
	uint8_t magic[2];
	uint8_t source;
	uint8_t destination;
	uint8_t ivt[LOWNET_IVT_SIZE];
	uint8_t protocol;
	uint8_t length;
	uint8_t padding[2];
	uint8_t payload[LOWNET_PAYLOAD_SIZE];
	uint32_t crc;
} lownet_secure_frame_t;

static_assert(sizeof(lownet_frame_t) == LOWNET_FRAME_SIZE, "lownet_frame_t size is incorrect");
static_assert(sizeof(lownet_secure_frame_t) == 228, "lownet_secure_frame_t size is incorrect");
static_assert(offsetof(lownet_secure_frame_t, payload) ==
                  LOWNET_UNENCRYPTED_SIZE + LOWNET_IVT_SIZE + 4,
              "Payload is incorrectly offset inside lownet_secure_frame_t");
static_assert((LOWNET_ENCRYPTED_SIZE % 16) == 0, "LOWNET_ENCRYPTED_SIZE must be a multiple of 16");
static_assert(LOWNET_UNENCRYPTED_SIZE + LOWNET_ENCRYPTED_SIZE ==
                  sizeof(lownet_frame_t),
              "LOWNET_UNENCRYPTED_SIZE + LOWNET_ENCRYPTED_SIZE does not match "
              "the size of lownet_frame_t");
static_assert(
    LOWNET_UNENCRYPTED_SIZE + LOWNET_IVT_SIZE + LOWNET_ENCRYPTED_SIZE ==
        sizeof(lownet_secure_frame_t),
    "LOWNET_UNENCRYPTED_SIZE + LOWNET_IVT_SIZE + LOWNET_ENCRYPTED_SIZE does "
    "not match the size of lownet_secure_frame_t");

typedef struct __attribute__((__packed__)) {
	uint32_t seconds; // Seconds since UNIX epoch.
	uint8_t parts; // Milliseconds, 1000/256 resolution.
} lownet_time_t;

static_assert(sizeof(lownet_time_t) == 5, "lownet_time_t size is incorrect");

typedef void (*lownet_recv_fn)(const lownet_frame_t* frame);

// Usage: lownet_register_protocol(PROTO, HANDLER)
// Pre:   PROTO is a protocol identifier which has not been registered
//        HANDLER is the frame handler for PROTO
// Value: 0 if PROTO was successfully registered, non-0 otherwise
int lownet_register_protocol(uint8_t protocol, lownet_recv_fn handler);

// Lownet key structure.  Bytes member MUST point to a usable contiguous
// region of memory of AT LEAST 'size' bytes.
typedef struct {
	uint8_t* bytes;
	uint32_t size;
} lownet_key_t;

typedef void (*lownet_recv_fn)(const lownet_frame_t* frame);
typedef void (*lownet_cipher_fn)(const lownet_secure_frame_t* in_frame, lownet_secure_frame_t* out_frame);

void lownet_init(
	lownet_cipher_fn encrypt_fn,
	lownet_cipher_fn decrypt_fn
);

void lownet_send(const lownet_frame_t* frame);

lownet_time_t lownet_get_time();
void lownet_set_time(const lownet_time_t* time);

uint8_t lownet_get_device_id();

const lownet_key_t* lownet_get_key();
void lownet_set_key(const lownet_key_t* key);
void lownet_set_stored_key(uint8_t key_id);

const char* lownet_get_signing_key();

#include "lownet_crypt.h"

// Some pre-shared lownet AES keys.
static const lownet_input_key_t base_shared_key = {{
	0xc0c71cc5, 0x748ce81a, 0x4b0e4aa7, 0x70c0d55e,
	0x58957e01, 0xed51d8cc, 0x26b844c4, 0x49c50530
}};
static const lownet_input_key_t alt_shared_key = {{
	0x0b7b9b81, 0x350ecef1, 0x7a7b0fbb, 0xe9f134d2,
	0x33bffa82, 0xc7c82730, 0x4861bba3, 0x44e44aba
}};

// Master node signing key.
static const char lownet_public_key[] =
"-----BEGIN PUBLIC KEY-----\n"
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxG9VF9wuocepQnwBkxUb\n"
"4YxCo1NJ1MAKAGoaK2csfPABSRkjlESev42rFVzejGtOp2pxKcyihDXVe1BEzD0q\n"
"HXxEgtkRy0/bJNhGxoMmWTbikO3BmIMIO9zIk3leaNtyy49U27CKDgUHOPp6zd3c\n"
"dgD3nE4fIE7tU3mCJ4xh5xMHeyoqa/MV3EkE9VDV2vCTP3KyKDFObYqig6XWydeQ\n"
"CPmSAr0rRYiriguOvQGGxPeaCWPaUAG+t2W7ydpeju+Dkzl6NHm0q9JdLfpg8zje\n"
"BgLekdFxyM4jAK2hCX+vswUrYqbm5m9rptxQUuSYpk27Ew7uWRaomAWWeMLIg+zt\n"
"rwIDAQAB\n"
"-----END PUBLIC KEY-----";

#endif
