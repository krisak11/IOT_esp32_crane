#ifndef LOWNET_UTIL_H
#define LOWNET_UTIL_H

#include "lownet.h"

// Usage: time_to_milliseconds(TIME)
// Pre:   TIME != NULL
// Value: The number of milliseconds represented by TIME
uint32_t time_to_milliseconds(const lownet_time_t* time);

// Usage: time_from_milliseconds(MILLIS)
// Pre:   None
// Value: The time value represented by MILLIS
lownet_time_t time_from_milliseconds(uint32_t millis);

// Usage: compare_time(LHS, RHS)
// Pre:   LSH != NULL, RHS != NULL
// Value: -1 if LSH is smaller than RHS
//         0 if LSH is equal to RHS
//         1 if LSH is greater than RHS
int compare_time(const lownet_time_t* lhs, const lownet_time_t* rhs);

// Usage: time_diff(A, B)
// Pre:   A != NULL, B != NULL,
//        B must be greater than A as defined by compare_time(A, B).
// Value: The difference between A and B
lownet_time_t time_diff(const lownet_time_t* a, const lownet_time_t* b);

// uint32 + '.' + uint32 + 's'
#define TIME_WIDTH (11 + 1 + 11 + 1)
// Usage: format_time(BUFFER, TIME)
// Pre:   BUFFER != NULL, TIME != NULL
//        sizeof BUFFER >= TIME_WIDTH
// Post:  TIME has been formatted into buffer
// Value: The number of characters written to BUFFER
int format_time(char* buffer, lownet_time_t* time);

#define ID_WIDTH 4
// Usage: format_id(BUFFER, ID)
// Pre:   BUFFER != NULL, sizeof BUFFER >= ID_WIDTH
// Post:  ID has been formatted into buffer
// Value: The number of characters written to BUFFER
int format_id(char* buffer, uint8_t id);


#endif
