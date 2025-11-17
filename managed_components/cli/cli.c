#include "cli.h"

#include <string.h>
#include <stdio.h>

command_fun_t find_command(const char* command, const command_t* commands, size_t n)
{
	/*
		Loop Invariant:
		0 <= i < n
		forall x | 0 <= x < i : commands[x].name != command
	 */
	for (size_t i = 0; i < n; ++i)
		if (!strcmp(command, commands[i].name))
			return commands[i].fun;

	return NULL;
}
