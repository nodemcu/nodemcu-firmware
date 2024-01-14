# Update source files and includes as necessary
idf_component_register(
  SRCS
    "mymod.c"
  PRIV_INCLUDE_DIRS
    "${CMAKE_CURRENT_BINARY_DIR}"
  PRIV_REQUIRES
    "base_nodemcu"
    "lua"
    "platform"
)

# To register the module with NodeMCU, use one of the below functions.
# The name given MUST match the first argument to the NODEMCU_MODULE() in
# the module's C file.
#
# For modules with a Kconfig option, use:
extmod_register_conditional(MYMOD)
# ...and for modules without a Kconfig option, instead use:
#extmod_register_unconditional(MYMOD)
