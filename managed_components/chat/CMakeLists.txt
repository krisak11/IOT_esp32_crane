idf_component_register(
	SRCS "chat.c"
	INCLUDE_DIRS "include"
	REQUIRES "serial" "lownet"
	PRIV_REQUIRES "utility"
)
