#ifndef GUARD_UTILITY_H
#define GUARD_UTILITY_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Usage: HEXDUMP(x)
// Pre:   x is a valid operand for the addressof operator (&)
// Post:  A hexadecimal representation of x has been written to
//        the serial port
#define HEXDUMP(x) do {\
	for (int i = 0; i < sizeof x; ++i)\
	printf("%02x%c", ((uint8_t*)&x)[i], (i + 1) % 16 == 0 ? '\n' : ' ');\
	putchar('\n');\
	} while (0)

int util_printable(char c);

// Usage: min(A, B)
// Pre:   None, other than those imposed by the type system
// Value: The smaller of A and B
uint8_t min(uint8_t a, uint8_t b);

uint32_t hex_to_dec(const char* hex_digits);

// Usage: buffers_compare(A, B, SIZE)
// Pre:   A != NULL, B != NULL
//        A and B are buffers of size SIZE
// Value: -1 if A is less than B,
//         0 if A and B are equal,
//         1 if A is greater than B
int buffers_compare(const uint8_t* a, const uint8_t* b, size_t size);

// Usage: buffers_equal(A, B, SIZE)
// Pre:   A != NULL, B != NULL
//        A and B are buffers of size SIZE
// Value: true if A and B are equal, false otherwise
bool buffers_equal(const uint8_t* a, const uint8_t* b, size_t size);

#endif
