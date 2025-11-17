#include "lownet-commands.h"

#include <stdio.h>

#include <lownet.h>
#include <lownet_util.h>
#include <serial_io.h>

void id_command(char*)
{
	// id + null
	char buffer[ID_WIDTH + 1];
	format_id(buffer, lownet_get_device_id());
	serial_write_line(buffer);
}

void date_command(char*)
{
	lownet_time_t time = lownet_get_time();
	if (time.seconds == 0 && time.parts == 0)
		{
			serial_write_line("Network time is not available.");
			return;
		}

	// time + description + null
	char buffer[TIME_WIDTH + 25 + 1];
	int n = format_time(buffer, &time);
	sprintf(buffer + n, " since the course started");
	serial_write_line(buffer);
}
