#define INCLUDE_vTaskDelete 1

#include "lownet.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <nvs_flash.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_wifi.h>

#include <device-table.h>

#define TAG "lownet-core"

#define EVENT_CORE_READY 0x01
#define EVENT_CORE_ERROR 0x02

#define TIMEOUT_STARTUP ((TickType_t)(5000 / portTICK_PERIOD_MS))

typedef struct {
	uint8_t protocol;
	lownet_recv_fn handler;
} protocol_t;

static uint8_t aes_key_bytes[LOWNET_KEY_SIZE_AES];

struct {
	TaskHandle_t  lownet_task;
	TaskHandle_t  decrypt_task;

	EventGroupHandle_t events;
	QueueHandle_t inbound;
	QueueHandle_t decrypt_queue;

	lownet_cipher_fn encrypt;
	lownet_cipher_fn decrypt;
	lownet_key_t aes_key;
	const char* signing_key;

	lownet_identifier_t identity;
	lownet_identifier_t broadcast;

	// Network timing details.
	lownet_time_t sync_time;
	int64_t sync_stamp;
	protocol_t protocols[LOWNET_MAX_PROTOCOLS];
	uint8_t num_protocols;
} net_system;

const uint8_t plain_magic[2] = {0x10, 0x4e};
const uint8_t cipher_magic[2] = {0x20, 0x4e};

uint8_t net_initialized = 0;

// Forward declarations.
lownet_recv_fn lownet_get_handler(uint8_t protocol);
void lownet_service_main(void* pvTaskParam);
void decrypt_service_main(void* pvTaskParam);
void lownet_service_kill();
void lownet_inbound_handler(const esp_now_recv_info_t * info, const uint8_t* data, int len);

void lownet_sync_time(const lownet_frame_t* time_frame);
uint32_t lownet_crc(const lownet_frame_t* frame);

void lownet_init(lownet_cipher_fn encrypt_fn, lownet_cipher_fn decrypt_fn) {
	if (net_initialized) {
		ESP_LOGE(TAG, "LowNet already initialized");
		return;
	} else {
		net_initialized = 1;
		memset(&net_system, 0, sizeof(net_system));
		net_system.aes_key.bytes = (uint8_t*)&aes_key_bytes;
	}

	net_system.decrypt_queue = xQueueCreate(16, sizeof(lownet_secure_frame_t));
	if (!net_system.decrypt_queue)
		{
			ESP_EARLY_LOGE(TAG, "Error creating lownet decrypt queue");
			return;
		}

	ESP_ERROR_CHECK(nvs_flash_init());        // initialize NVS
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	if (esp_now_init() != ESP_OK) {
		ESP_LOGE(TAG, "Error initializing ESP-NOW");
		return;
	}

	net_system.encrypt = encrypt_fn;
	net_system.decrypt = decrypt_fn;

	net_system.events = xEventGroupCreate();
	if (!net_system.events) {
		ESP_LOGE(TAG, "Error creating lownet event group");
		return;
	}

	// Initialize the keystore.
	lownet_keystore_init();

	// Pre-fill the keystore with the well-known keys.
	lownet_keystore_write(0, &base_shared_key);
	lownet_keystore_write(1, &alt_shared_key);

	// Apply the signing key.
	net_system.signing_key = lownet_public_key;

	// Create the primary network service task.
	xTaskCreatePinnedToCore(
		lownet_service_main,
		"lownet_service",
		4096,
		NULL, // Pass no params into task method.
		LOWNET_SERVICE_PRIO,
		&net_system.lownet_task,
		LOWNET_SERVICE_CORE
	);

	xTaskCreate(
		decrypt_service_main,
		"decrypt_service",
		2048,
		NULL,
		LOWNET_SERVICE_PRIO,
		&net_system.decrypt_task
	);

	// Block until the service has finished its own startup and is ready to use.
	EventBits_t startup_result = xEventGroupWaitBits(
		net_system.events,
		(EVENT_CORE_READY | EVENT_CORE_ERROR),
		pdFALSE,
		pdFALSE,
		TIMEOUT_STARTUP
	);

	if (startup_result & EVENT_CORE_ERROR) {
		ESP_LOGE(TAG, "Error starting network service..");
		vEventGroupDelete(net_system.events);
		lownet_keystore_free();
		net_initialized = 0;
		return;
	}
	else if (!(startup_result & EVENT_CORE_READY)) {
		ESP_LOGE(TAG, "Timed out waiting for network service startup...");
		vEventGroupDelete(net_system.events);
		lownet_keystore_free();
		net_initialized = 0;
		return;
	}

	ESP_LOGI(TAG, "Initialized LowNet -- device ID: 0x%02X", net_system.identity.node);
}


// Delegation method for encrypting and sending a lownet frame.  Presume only
//	lownet internal usage, so relaxed precondition check.
void lownet_encrypt_send(const lownet_frame_t* frame) {
	lownet_secure_frame_t plain;
	lownet_secure_frame_t cipher;

	// Little sanity check; IVT size must be multiple of 16 in current
	// implementation.
	#if LOWNET_IVT_SIZE % 16 != 0
	#error "IVT  size violation"
	#endif

	// Generate the initialization vector and padding entropy.
	for (int i = 0; i < LOWNET_IVT_SIZE / 4; ++i) {
		((uint32_t*)plain.ivt)[i] = esp_random();
	}

	// Clone the plaintext frame into the plaintext secure frame.
	memcpy(&plain.magic, cipher_magic, 2);
	plain.source = frame->source;
	plain.destination = frame->destination;
	memcpy(&plain.protocol, &frame->protocol, LOWNET_ENCRYPTED_SIZE);

	// Encrypt with user-defined enc function.
	net_system.encrypt(&plain, &cipher);

	if (esp_now_send(net_system.broadcast.mac, (const uint8_t*)&cipher, sizeof(cipher)) != ESP_OK) {
		ESP_LOGE(TAG, "LowNet Frame send error");
	}
}


// Public interface; standard send.  Delegate to encrypt-and-send method if AES encryption
// key is defined.
void lownet_send(const lownet_frame_t* frame) {
	// Discard packet instead of sending if specified payload length
	// is impossible.
	if (frame->length > LOWNET_PAYLOAD_SIZE) { return; }

	lownet_frame_t out_frame;
	memset(&out_frame, 0, sizeof(out_frame));

	memcpy(&out_frame.magic, plain_magic, 2);
	out_frame.source = net_system.identity.node; // Overwrite frame source with device ID.
	out_frame.destination = frame->destination;
	out_frame.protocol = frame->protocol;
	out_frame.length = frame->length;
	memcpy(out_frame.payload, frame->payload, frame->length);
	for (int i = frame->length; i < LOWNET_PAYLOAD_SIZE; ++i) {
		// Fill any unused payload with noise.  Improves packet entropy
		// for encryption purposes etc.
		out_frame.payload[i] |= (uint8_t)(esp_random() & 0x000000FF);
	}

	// Generate and apply the lownet CRC to the frame.
	out_frame.crc = lownet_crc(&out_frame);

	if (lownet_get_key() != NULL) {
		// We have an AES key -- use it to encrypt the frame.
		lownet_encrypt_send(&out_frame);
	} else {
		// No key is active -- send the frame as-is, plaintext.
		if (esp_now_send(net_system.broadcast.mac, (const uint8_t*)&out_frame, sizeof(out_frame)) != ESP_OK) {
			ESP_LOGE(TAG, "LowNet Frame send error");
		}
	}
}

// Formats and returns a lownet time structure based on synced network time.
lownet_time_t lownet_get_time() {
	lownet_time_t result;
	memset(&result, 0, sizeof(result));

	if (net_system.sync_time.seconds == 0) {
		// Haven't received a timesync yet.  Can't do anything useful.
		return result;
	}

	int64_t delta = ((esp_timer_get_time() / 1000) - net_system.sync_stamp)
					+ ((((int64_t)net_system.sync_time.parts) * 1000) / 256);

	result.seconds = net_system.sync_time.seconds + (uint32_t)(delta / 1000);
	result.parts = (uint8_t)(((delta % 1000) * 256) / 1000);

	return result;
}


// Sets the network time based on a given network time.
void lownet_set_time(const lownet_time_t* time) {
	// Overwrite the network time with the received time.  Take the processor timestamp at
	// this moment.
	memcpy(&net_system.sync_time, time, sizeof(lownet_time_t));
	net_system.sync_stamp = (esp_timer_get_time() / 1000);
}


// Returns the ID of this device.
uint8_t lownet_get_device_id() {
	return net_system.identity.node;
}


// Returns the currently active AES key, or NULL if no key is in use.
const lownet_key_t* lownet_get_key() {
	if (net_system.aes_key.size == 0) {
		return NULL;
	}
	return &net_system.aes_key;
}


// Sets the AES key.  Pass in NULL to disable encryption.  If an actual key is
//	provided, the size must match the expected key size.
void lownet_set_key(const lownet_key_t* key) {
	if (key == NULL) {
		// Disable AES.
		net_system.aes_key.size = 0;
		return;
	}
	if (key->size != LOWNET_KEY_SIZE_AES) {
		ESP_LOGE(TAG, "Invalid AES key size");
		return;
	}
	net_system.aes_key.size = LOWNET_KEY_SIZE_AES;
	memcpy(net_system.aes_key.bytes, key->bytes, net_system.aes_key.size);
}


// Sets the AES key from an existing stored key.
void lownet_set_stored_key(uint8_t key_id) {
	lownet_key_t stored_key = lownet_keystore_read(key_id);
	lownet_set_key(&stored_key);
}


// Return the signing (public) RSA key PEM.
const char* lownet_get_signing_key() {
	return net_system.signing_key;
}

void decrypt_service_main(void* pvTaskParam)
{
	while (true)
		{
			lownet_secure_frame_t cipher;
			lownet_secure_frame_t plain;
			memset(&cipher, 0, sizeof cipher);

			if (xQueueReceive(net_system.decrypt_queue , &cipher, UINT32_MAX) != pdTRUE)
				continue;

			net_system.decrypt(&cipher, &plain);
			lownet_frame_t frame;
			memcpy(&frame, &plain, LOWNET_UNENCRYPTED_SIZE);
			memcpy(&frame.magic, plain_magic, sizeof plain_magic);
			memcpy(&frame.protocol, &plain.protocol, LOWNET_ENCRYPTED_SIZE);
			xQueueSend(net_system.inbound, &frame, 0);
		}
}


// NOTE: Task methods do not return as a rule of thumb; anywhere we
// encounter an error state we set the core error bit and kill the
// service.  Note we include a 'return;' after such a service kill,
// but these lines should never execute.
void lownet_service_main(void* pvTaskParam) {
	// Create an inbound packet queue.
	net_system.inbound = xQueueCreate(16, sizeof(lownet_frame_t));
	if (!net_system.inbound) {
		ESP_EARLY_LOGE(TAG, "Error creating lownet inbound packet queue");
		lownet_service_kill();
		return;
	}

	// Figure out our device identity, and the broadcast identity.
	uint8_t local_mac[6];
	esp_read_mac(local_mac, ESP_MAC_WIFI_STA);

	net_system.identity = lownet_lookup_mac(local_mac);
	net_system.broadcast = lownet_lookup(0xFF);

	if (!net_system.identity.node || !net_system.broadcast.node) {
		ESP_EARLY_LOGE(TAG, "Failed to identify device / broadcast identity");
		lownet_service_kill();
		return;
	}

	// Register the broadcast address.
	esp_now_peer_info_t peer_info = {};
	memcpy(peer_info.peer_addr, net_system.broadcast.mac, 6);
	peer_info.channel = 0;
	peer_info.ifidx = ESP_IF_WIFI_STA;
	peer_info.encrypt = false;
	if (esp_now_add_peer(&peer_info) != ESP_OK) {
		ESP_EARLY_LOGE(TAG, "Failed to add broadcast peer address");
		lownet_service_kill();
		return;
	}

	// Register our inbound network callback.
	esp_now_register_recv_cb(lownet_inbound_handler);

	// Initialization done.  Set the ready bit and then hang  around
	// dispatching frames as they arrive.
	xEventGroupSetBits(
		net_system.events,
		EVENT_CORE_READY
	);


	while (1) {
		lownet_frame_t  frame;
		memset(&frame, 0, sizeof(frame));

		// Blocking call to receive from inbound queue.  Task will be blocked until
		// queue has data for us.
		if (xQueueReceive(net_system.inbound, &frame, UINT32_MAX) == pdTRUE) {

			if (memcmp(&frame.magic, plain_magic, 2) != 0)
				{
					ESP_LOGD(TAG, "Invalid magic bytes");
					continue;
				}

			// Check whether the network frame checksum matches computed checksum.
			if (lownet_crc(&frame) != frame.crc)
				{
					ESP_LOGD(TAG, "CRC error");
					continue;
				}

			// Not strictly to spec but a useful safety valve; if frame has, as a source
			// address, the broadcast address, discard it -- something has gone wrong.
			if (frame.source == 0xFF) { continue; }

			// Check whether packet destination is us or broadcast.
			if (frame.destination != net_system.identity.node && frame.destination != net_system.broadcast.node)
				{
					continue;
				}

			lownet_recv_fn handler = lownet_get_handler(frame.protocol & 0b00111111);
			if (!handler)
				{
					ESP_LOGD(TAG, "Unknown protocol %02x", frame.protocol & 0b00111111);
					continue;
				}

			handler(&frame);
		}
	}
}

// Kills the lownet service task and allows for lownet re-initialization.
void lownet_service_kill() {
	xEventGroupSetBits(net_system.events, EVENT_CORE_ERROR);

	if (net_system.inbound) {
		vQueueDelete(net_system.inbound);
	}
	vTaskDelete(net_system.lownet_task);
	vTaskDelete(net_system.decrypt_task);
	return; // Should never execute, when this function is called from lownet service.
}

// Inbound frame callback is executed from the context of the ESPNOW task!
// It is of great importance that this callback function not block, and
// return quickly to avoid locking up the wifi driver.
void lownet_inbound_handler(const esp_now_recv_info_t * info, const uint8_t* data, int len) {
	if (len == sizeof(lownet_frame_t) && net_system.aes_key.size == 0) {
		// Non-blocking queue send; if queue is full then packet is dropped.
		if (xQueueSend(net_system.inbound, data, 0) != pdTRUE) {
			// Error queueing data, likely errQUEUE_FULL.
			// Packet is dropped.
		}
	} else if (len == sizeof(lownet_secure_frame_t) && net_system.aes_key.size != 0) {
		xQueueSend(net_system.decrypt_queue, data, 0);
	}
}

void lownet_sync_time(const lownet_frame_t* time_frame) {
	if (lownet_get_key())
		// Encryption is enabled, ignore time packets.
		return;

	if (time_frame->length != sizeof(lownet_time_t)) {
		// Malformed time packet, do nothing.
		return;
	}

	memcpy(&net_system.sync_time, time_frame->payload, sizeof(lownet_time_t));
	net_system.sync_stamp = (esp_timer_get_time() / 1000);
}

uint32_t lownet_crc(const lownet_frame_t* frame) {
	uint32_t reg = 0x00777777; // Shift register initial vector.
	static const uint32_t poly = 0x1800463ul;  // G(x)

		void process_byte(uint8_t b)
		{
				for(int i=0; i<8; i++)
				{
						reg = (reg<<1) | (b&1);
						b = b>>1;
						if ( reg & 0x1000000ul )
								reg = (reg^poly); // take mod G(x)
				}
		}
	const uint8_t* iter = (const uint8_t*)frame;
	int len = LOWNET_FRAME_SIZE - LOWNET_CRC_SIZE;
		for(int i=0; i<len; i++)
				process_byte( iter[i] );
	return reg;

}

// Usage: lownet_register_protocol(PROTO, HANDLER)
// Pre:   PROTO is a protocol identifier which has not been registered
//        HANDLER is the frame handler for PROTO
// Value: 0 if PROTO was successfully registered, non-0 otherwise
// Post:  An entry has been added to net_system.protocols and
//        net_system.num_protocols has been incremented by 1
int lownet_register_protocol(uint8_t protocol, lownet_recv_fn handler)
{
	if (net_system.num_protocols >= LOWNET_MAX_PROTOCOLS)
		return 1;

	net_system.protocols[net_system.num_protocols].protocol = protocol;
	net_system.protocols[net_system.num_protocols].handler = handler;

	++net_system.num_protocols;
	return 0;
}

// Usage: lownet_get_handler(PROTO)
// Pre:   PROTO is a protocol identifier which has previously been
//        registered with lownet_register_protocol
// Value: The frame handler for PROTO
lownet_recv_fn lownet_get_handler(uint8_t protocol)
{
	/*
	 * Loop invariant:
	 * 0 <= i <= net_system.num_protocols
	 * forall x 0 <= x < i | net_system.protocols[x].protocol != protocol
	 */
	for (int i = 0; i < net_system.num_protocols; ++i)
		if (net_system.protocols[i].protocol == protocol)
			return net_system.protocols[i].handler;

	return NULL;
}
