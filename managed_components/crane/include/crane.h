/*****************************************************************
 *  For CLI:
 *  -------
 *  /crane open [#] | close  : open/close connection
 *  /crane u|d|f|b|o|O       : manual mode
 *  /crane test [node id]    : run test pattern
 *
 *  Status:   every sec if new data
 *  - otherwise every 15 seconds
 *****************************************************************/

#ifndef CRANE_H
#define CRANE_H

#include <stdint.h>

/*
 * Packet types
 */
#define  CRANE_CONNECT   0x01
#define  CRANE_STATUS    0x02
#define  CRANE_ACTION    0x03
#define  CRANE_CLOSE     0x04

/*
 * Flag bits
 */
#define  CRANE_SYN       (1<<0)
#define  CRANE_ACK       (1<<1)
#define  CRANE_NAK       (1<<2)
#define  CRANE_TEST      (1<<3) // start the test pattern

/*
 *  Actions
 */
#define  CRANE_NULL      0x00   // no action, test frame, "processed" immediately
#define  CRANE_STOP      0x01   // stop & flush queue immediately
#define  CRANE_FWD       0x02   // move forward
#define  CRANE_REV       0x03   // reverse
#define  CRANE_UP        0x04   // move up
#define  CRANE_DOWN      0x05   // move down
#define  CRANE_LIGHT_ON  0x06   // switch the light on
#define  CRANE_LIGHT_OFF 0x07   // and off

/*
 *  Initial connection establishment
 */
typedef struct __attribute__((__packed__))
{
	uint32_t challenge;
} conn_t;

/*
 * Status (ACK) messages from crane
 */
typedef struct __attribute__((__packed__))
{
	uint8_t backlog;   // actions in queue
	uint8_t time_left; // in seconds
	uint8_t light;     // light status:0 or 1
	int8_t  temp;      // temperature
} status_t;

/*
 *  Action messages from node to crane
 */
typedef struct __attribute__((__packed__))
{
	uint8_t cmd;
	uint8_t reserved[3];
} action_t;


/*
 *  Register:  challenge + flags SYN/ACK/NAK/TEST
 *  Status:    seq-ack + queue_length
 *  Action:    cmd
 *  Quit:      ACK
 */
typedef struct __attribute__((__packed__))
{
	uint8_t  type;
	uint8_t  flags;
	uint16_t seq;
	union
{
	conn_t    conn;   // connection establishment
	action_t  action; // action packets
	status_t  status; // status packets
	uint32_t  close;  // reserved - nulls
} d;
} crane_packet_t;

/*******************************************************************************************/

int crane_init(void);
void crane_command(char* args);

#endif
