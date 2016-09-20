# Match up all the module source files with their corresponding Kconfig
# option in the form LUA_MODULE_<modname> and if enabled, add a
# "-u <modname>_module_selected1" option to force the linker to include
# the module. See components/core/include/module.h for further details on
# how this works.
-include $(PROJECT_PATH)/build/include/config/auto.conf
include $(PROJECT_PATH)/components/modules/uppercase.mk

MODULE_NAMES:=$(call uppercase,$(subst .c,,$(wildcard *.c)))
FORCE_LINK:=$(foreach mod,$(MODULE_NAMES),$(if $(CONFIG_LUA_MODULE_$(mod)), -u $(mod)_module_selected1))
COMPONENT_ADD_LDFLAGS=$(FORCE_LINK) -lmodules

include $(IDF_PATH)/make/component_common.mk
