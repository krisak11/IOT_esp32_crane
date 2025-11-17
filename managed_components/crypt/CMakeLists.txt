idf_component_register(
    SRCS "crypt.c"
    INCLUDE_DIRS "include"
    REQUIRES "lownet"
    PRIV_REQUIRES "serial" "mbedtls"
)
