#ifndef LOWNET_COMMANDS_H
#define LOWNET_COMMANDS_H

// DEFINITION: A node id is a string of the form 0xXX where XX is a
// valid hexadecimal number.

// Usage: id_command(NULL)
// Pre:   None, this command takes no arguments.
// Post:  The id of the node has been written to the serial port.
void id_command(char* args);

// Usage: date_command(NULL)
// Pre:   None, this command takes no arguments.
// Post:  The network time has been written to the serial port.
void date_command(char* args);

#endif
