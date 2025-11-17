#ifndef CRYPT_H
#define CRYPT_H

#include "lownet.h"

void crypt_decrypt(const lownet_secure_frame_t* cipher, lownet_secure_frame_t* plain);
void crypt_encrypt(const lownet_secure_frame_t* plain, lownet_secure_frame_t* cipher);

// Usage: crypt_setkey_command(KEY)
// Pre:   KEY is NULL,  0, 1, or a AES key
// Post: If key was NULL encryption has been disabled.  If key was 0
//        or 1 the corresponding predefined key has been set as
//        active.  Otherwise KEY has been set as the active key.
// Note:  If key is shorter than LOWNET_KEY_SIZE_AES it will be padded
//        with zeroes.
void crypt_setkey_command(char* args);

// Usage: crypt_test_command(STR)
// Pre:   STR is a string
// Post:  The STR has been encrypted and then decrypted
//        and the result written to the serial port.
void crypt_test_command(char* str);

#endif
