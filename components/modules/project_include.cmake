# Helper functions for external module registration.
# - extmod_register_conditional() for modules with a Kconfig option
# - extmod_register_unconditional() for always-enabled modules

function(extmod_register_conditional confname)
  if (${CONFIG_NODEMCU_CMODULE_${confname}})
    # If the module is enabled in menuconfig, add the linker option
    # "-u <confname>_module_selected1" to make the linker include this
    # module. See components/core/include/module.h for further details
    # on how this works.
    message("Including external module ${confname}")
    target_link_libraries(${COMPONENT_LIB} "-u ${confname}_module_selected1")
  endif()
endfunction()

function(extmod_register_unconditional confname)
  message("Including external module ${confname}")
  # The module macros rely on the presence of a CONFIG_NODEMCU_CMODULE_XXX
  # def, so we have to add it explicitly as it won't be coming from Kconfig
  target_compile_options(${COMPONENT_LIB} PRIVATE "-DCONFIG_NODEMCU_CMODULE_${confname}")
  target_link_libraries(${COMPONENT_LIB} "-u ${confname}_module_selected1")
endfunction()
