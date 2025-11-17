#include "utility.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>

int util_printable(char c) {
	return ((c >= ' ' && c < 127) ? 1 : 0);
}

uint8_t min(uint8_t a, uint8_t b)
{
	return (a <= b) ? a : b;
}

uint32_t hex_to_dec(const char* hex_digits) {
	const char* map = "0123456789abcdef";
	uint32_t acc = 0x00000000;

	for (int i = 0; i < strlen(hex_digits); ++i) {
		uint32_t addend = 0x10; // Too large for single digit, sentinel value.
		for (int j = 0; j < 16; ++j) {
			if (tolower(hex_digits[i]) == map[j]) {
				addend = j;
				break;
			}
		}
		if (addend > 0x0F) {
			// Invalid digit.
			return 0;
		}
		acc = (acc << 4) + addend;
	}
	return acc;
}

int buffers_compare(const uint8_t* a, const uint8_t* b, size_t size)
{
	return memcmp(a, b, size);
}

bool buffers_equal(const uint8_t* a, const uint8_t* b, size_t size)
{
	return buffers_compare(a, b, size) == 0;
}
