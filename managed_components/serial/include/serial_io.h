#ifndef SERIAL_IO_H
#define SERIAL_IO_H

#define MSG_BUFFER_LENGTH 128

#define SERIAL_SERVICE_CORE 0
#define SERIAL_SERVICE_PRIO 4

void serial_write_line(const char* string);
int serial_read_line(char* buffer);


// Serial rebuild, to Task-based design.
void init_serial_service();

#endif
