// CSTDLIB includes.
#include <stdio.h>
#include <string.h>

// FreeRTOS includes.
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_log.h>

// Custom includes.
#include "serial_io.h"

#define TAG "serial_io.c"


// Constants and definitions.
const TickType_t  service_delay = 50 / portTICK_PERIOD_MS;
const TickType_t  timeout_read = 5000 / portTICK_PERIOD_MS;
const char* PROMPT_TOKEN = "> ";
const char* MESSAGE_SYNC = "SYNC // FIRMWARE READY";

// Internal data structures

void svc_serial(void* pvTaskParams);

struct {
	QueueHandle_t   queue_write;
	QueueHandle_t   queue_read;
	TaskHandle_t  service;
} serial_system;

void init_serial_service() {
	memset(&serial_system, 0, sizeof(serial_system));

	serial_system.queue_write = xQueueCreate(16, MSG_BUFFER_LENGTH);
	serial_system.queue_read = xQueueCreate(4, MSG_BUFFER_LENGTH);
	if (!serial_system.queue_write || !serial_system.queue_read) {
		ESP_LOGE(TAG, "Failed to create serial message queues");
		return;
	}

	xTaskCreatePinnedToCore(
		svc_serial,
		"serial_io_service",
		4096,
		NULL,
		SERIAL_SERVICE_PRIO,
		&serial_system.service,
		SERIAL_SERVICE_CORE
	);

	serial_write_line(MESSAGE_SYNC);
}


void svc_serial(void* pvTaskParams) {
	char in_msg[MSG_BUFFER_LENGTH];
	char out_msg[MSG_BUFFER_LENGTH];

	memset(in_msg, 0, MSG_BUFFER_LENGTH);
	memset(out_msg, 0, MSG_BUFFER_LENGTH);

	int done = 0;
	int at = 0;

	while (1) {
		while (xQueueReceive(serial_system.queue_write, out_msg, 0) == pdTRUE) {
			// We can only write up to LEN - 2 characters; penultimate char would
			// be newline character '\n' and ultimate char is null terminator '\0'.
			char msg_buffer[MSG_BUFFER_LENGTH];

			int end = strlen(out_msg);
			if (end > MSG_BUFFER_LENGTH - 2) {
				end = MSG_BUFFER_LENGTH - 2;
			}
			strncpy(msg_buffer, out_msg, end);

			// Add a newline and null terminator to the input.
			if (strncmp(msg_buffer, PROMPT_TOKEN, 2)) {
				msg_buffer[end] = '\n';
				msg_buffer[end+1] = '\0';
			} else {
				msg_buffer[end] = '\0';
			}

			// Write the contents of the internal buffer to standard out (serial output
			// in this case).
			printf(msg_buffer);
			fflush(stdout);

			memset(out_msg, 0, MSG_BUFFER_LENGTH);
		}

		if (!done) {
			// Grab the next char from the serial input stream.
			int next = fgetc(stdin);

			// If the result is EOF, then there was nothing there (yet).  In this
			// circumstance, sleep a while before checking again.
			if (next == EOF) {
				vTaskDelay(service_delay);
				continue;
			}

			if ((char)next == '\n') {
				// Terminating newline character found.
				done = 1;
			} else if (at < (MSG_BUFFER_LENGTH - 1)) {
				in_msg[at++] = (char)next;
			} else {
				// We've overrun the internal buffer.  Additional characters
				// read until a newline is encountered are dumped.
				at++;
			}
		}

		// We've caught an input terminator, and the input message is done.
		if (done) {
			xQueueSend(serial_system.queue_read, in_msg, 0);
			memset(in_msg, 0, MSG_BUFFER_LENGTH);

			done = 0;
			at = 0;
		}
	}
}


// Serial IO function implementations.

// Writes a string to the serial output.  Non-blocking.
void serial_write_line(const char* string) {
	// Pre-condition: Input string may not be NULL.
	if (string == NULL) { return; }

	char msg_buffer[MSG_BUFFER_LENGTH];
	memset(msg_buffer, 0, MSG_BUFFER_LENGTH);
	int len = strlen(string);
	len = (len > (MSG_BUFFER_LENGTH - 2) ? MSG_BUFFER_LENGTH - 2 : len);
	strncpy(msg_buffer, string, len);
	msg_buffer[len] = '\0';

	xQueueSend(serial_system.queue_write, msg_buffer, 0);
}

// Blocking method, reads a string from serial input.
int serial_read_line(char* buffer) {
	// Precondition: output buffer may not be NULL.
	if (buffer == NULL) { return 0; }

	if (xQueueReceive(serial_system.queue_read, buffer, timeout_read) == pdTRUE) {
		return 0;
	}

	// No input, timed out.
	return -1;
}
