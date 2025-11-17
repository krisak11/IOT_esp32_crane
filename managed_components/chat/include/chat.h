#ifndef CHAT_H
#define CHAT_H

#include <stdint.h>

#include "lownet.h"

#define LOWNET_PROTOCOL_CHAT 0x02

void chat_init();

// Usage: shout_command(MSG)
// Pre:   MSG != NULL
// Post:  MSG as been broadcast over the network.
void shout_command(char* args);

// Usage: tell_command(ARGS)
// Pre:   ARGS is a string of the form 'ID MSG'
//        where ID is a node id number and MSG is a non
//        empty string.  ID and MSG must be separated by a single space.
// Post:  MSG has been sent to the node identified by ID.
void tell_command(char* args);

void chat_receive(const lownet_frame_t* frame);

void chat_shout(const char* message);
void chat_tell(const char* message, uint8_t destination);

#endif
