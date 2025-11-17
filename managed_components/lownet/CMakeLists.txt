idf_component_register(
	SRCS "lownet.c" "lownet_crypt.c" "lownet_util.c"
	INCLUDE_DIRS "include"
	REQUIRES "device-table" "esp_wifi" "nvs_flash" "esp_timer" "mbedtls"
)
