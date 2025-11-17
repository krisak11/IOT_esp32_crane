idf_component_register(
  SRCS "crane.c"
  INCLUDE_DIRS "include"
  REQUIRES "lownet"
  PRIV_REQUIRES "utility" "serial"
)
