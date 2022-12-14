# Modify this list as necessary to include all your source files.
set(extmod_srcs
  "mymod.c"
)

# If necessary, add items to the PRIV_REQUIRES list below
idf_component_register(
  SRCS ${extmod_srcs}
  INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
  PRIV_REQUIRES
    "base_nodemcu"
    "lua"
    "platform"
)

# The remainder is boiler-plate glue to get the linker to actually include
# the modules enabled in Kconfig. No user-serviceable parts inside.

# Match up all the extmod source files with their corresponding Kconfig
# option in the form NODEMCU_CMODULE_<modname> and if enabled, add a
# "-u <modname>_module_selected1" option to force the linker to include
# the module. See components/core/include/module.h for further details on
# how this works.
set(extmods_enabled)
foreach(extmod_src ${extmod_srcs})
  string(REPLACE ".c" "" module_name ${extmod_src})
  string(TOUPPER ${module_name} module_ucase)
  set(mod_opt "CONFIG_NODEMCU_CMODULE_${module_ucase}")
  if (${${mod_opt}})
    list(APPEND extmods_enabled ${module_ucase})
  endif()
endforeach()
message("Including the following modules: ${extmods_enabled}")

foreach(mod ${extmods_enabled})
  target_link_libraries(${COMPONENT_LIB} "-u ${mod}_module_selected1")
endforeach()
