idf_component_register(
  SRCS "app_main.c"
  INCLUDE_DIRS "."
  REQUIRES "lownet" "chat" "ping" "cli" "serial" "crypt" "command" "lownet-commands"
)
