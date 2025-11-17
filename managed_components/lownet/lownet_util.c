#include <stdio.h>

#include "lownet_util.h"

uint32_t time_to_milliseconds(const lownet_time_t* time)
{
	return time->seconds * LOWNET_TIME_RESOLUTION + time->parts;
}

lownet_time_t time_from_milliseconds(uint32_t millis)
{
	lownet_time_t time;
	time.seconds = millis / LOWNET_TIME_RESOLUTION;
	time.parts = millis % LOWNET_TIME_RESOLUTION;

	return time;
}

int compare_time(const lownet_time_t* lhs, const lownet_time_t* rhs)
{
	if (lhs->seconds < rhs->seconds)
		return -1;
	else if (lhs->seconds > rhs->seconds)
		return 1;
	else
		{
			if (lhs->parts < rhs->parts)
				return -1;
			else if (lhs->parts > rhs->parts)
				return 1;
			else
				return 0;
		}
}

lownet_time_t time_diff(const lownet_time_t* a, const lownet_time_t* b)
{
	return time_from_milliseconds(time_to_milliseconds(b) - time_to_milliseconds(a));
}

int format_time(char* buffer, lownet_time_t* time)
{
	return sprintf(buffer, "%lu.%lus", time->seconds, ((uint32_t)time->parts * 1000) / 256);
}

int format_id(char* buffer, uint8_t id)
{
	return sprintf(buffer, "0x%x", id);
}
