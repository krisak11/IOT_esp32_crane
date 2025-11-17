#ifndef SIGNATURE_H
#define SIGNATURE_H

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#define CMD_BLOCK_SIZE 256
typedef struct {
	uint8_t bytes[CMD_BLOCK_SIZE];
} signature_t;
static_assert(sizeof(signature_t) == CMD_BLOCK_SIZE, "signature_t size");

// Usage: signature_equal(A, B)
// Pre:   A != NULL, B != NULL
// Value: true if A and B are equal, false otherwise
bool signature_equal(const signature_t* a, const signature_t* b);

#endif
