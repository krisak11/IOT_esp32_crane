#ifndef GUARD_LOWNET_CRYPT_H
#define GUARD_LOWNET_CRYPT_H

#define AES_KEYSTORE_SIZE 4

#include "lownet.h"

// Structure for convenience, allows for a nice inline literal definition
//	for AES keys.
typedef struct {
	uint32_t words[LOWNET_KEY_SIZE_AES / 4];
} lownet_input_key_t;

// Key Store API
void lownet_keystore_init();
void lownet_keystore_free();

void lownet_keystore_write(uint8_t index, const lownet_input_key_t* input_key);
lownet_key_t lownet_keystore_read(uint8_t index);

#endif
