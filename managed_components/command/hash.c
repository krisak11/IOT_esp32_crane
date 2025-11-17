#include "hash.h"

#include <mbedtls/sha256.h>

#include <utility.h>

int hash(const char* data, size_t length, hash_t* hash)
{
	return mbedtls_sha256(
		(const unsigned char*) data,
		length,
		(unsigned char*) hash->bytes,
		0
	);
}

int hash_compare(const hash_t* a, const hash_t* b)
{
	return buffers_compare(a->bytes, b->bytes, sizeof(hash_t));
}

bool hash_equal(const hash_t* a, const hash_t* b)
{
	return hash_compare(a, b) == 0;
}
