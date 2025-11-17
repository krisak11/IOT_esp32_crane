#include "signature.h"

#include <utility.h>

bool signature_equal(const signature_t* a, const signature_t* b)
{
	return buffers_equal(a->bytes, b->bytes, sizeof(signature_t));
}
