#include <string.h>

#include "lownet.h"

lownet_key_t aes_keystore[AES_KEYSTORE_SIZE];
uint8_t keystore_init = 0;

void lownet_keystore_init() {
	if (keystore_init) { return; }

	for (int i = 0; i < AES_KEYSTORE_SIZE; ++i) {
		aes_keystore[i].size = 0;
		aes_keystore[i].bytes = malloc(LOWNET_KEY_SIZE_AES);
		memset(aes_keystore[i].bytes, 0, LOWNET_KEY_SIZE_AES);
	}

	keystore_init = 1;
}

void lownet_keystore_free() {
	if (!keystore_init) { return; }

	for (int i = 0; i < AES_KEYSTORE_SIZE; ++i) {
		free(aes_keystore[i].bytes);
	}

	keystore_init = 0;
}

void lownet_keystore_write(uint8_t index, const lownet_input_key_t* input_key) {
	if (!keystore_init || index >= AES_KEYSTORE_SIZE) { return; }

	memcpy(aes_keystore[index].bytes, input_key, LOWNET_KEY_SIZE_AES);
	aes_keystore[index].size = LOWNET_KEY_SIZE_AES;
}

lownet_key_t lownet_keystore_read(uint8_t index) {
	if (!keystore_init || index >= AES_KEYSTORE_SIZE) { return (lownet_key_t){0, 0}; }

	return aes_keystore[index];
}
