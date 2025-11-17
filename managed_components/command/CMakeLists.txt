idf_component_register(
	SRCS "command.c" "hash.c" "signature.c"
	INCLUDE_DIRS "include"
	REQUIRES "lownet"
	PRIV_REQUIRES "ping" "utility" "mbedtls"
)
