#ifndef HASH_H
#define HASH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define CMD_HASH_SIZE 32

typedef struct {
	uint8_t bytes[CMD_HASH_SIZE];
} hash_t;

static_assert(sizeof(hash_t) == CMD_HASH_SIZE, "hash_t size");

// Usage: hash(DATA, LENGTH, HASH)
// Pre:   DATA != NULL, HASH != NULL
//        DATA is a buffer of length LENGTH
// Post:  HASH contains the hash of DATA
// Value: 0 if hashing succeeded, non-0 otherwise
// Note: For the meaning of non-0 return values consult the
//       documentation of mbedtls_sha256
int hash(const char* data, size_t length, hash_t* hash);

// Usage: hash_compare(A, B, SIZE)
// Pre:   A != NULL, B != NULL
// Value: -1 if A is less than B,
//         0 if A and B are equal,
//         1 if A is greater than B
int hash_compare(const hash_t* a, const hash_t* b);

// Usage: hash_equal(A, B)
// Pre:   A != NULL, B != NULL
// Value: true if A and B are equal, false otherwise
bool hash_equal(const hash_t* a, const hash_t* b);
#endif
