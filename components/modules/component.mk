# Match up all the module source files with their corresponding Kconfig
# option in the form LUA_MODULE_<modname> and if enabled, add a
# "-u <modname>_module_selected1" option to force the linker to include
# the module. See components/core/include/module.h for further details on
# how this works.
-include $(PROJECT_PATH)/build/include/config/auto.conf
include $(PROJECT_PATH)/components/modules/uppercase.mk

MODULE_NAMES:=$(call uppercase,$(subst .c,,$(wildcard *.c)))
FORCE_LINK:=$(foreach mod,$(MODULE_NAMES),$(if $(CONFIG_LUA_MODULE_$(mod)), -u $(mod)_module_selected1))
COMPONENT_ADD_LDFLAGS=$(FORCE_LINK) -lmodules $(if $(CONFIG_LUA_MODULE_BTHCI),-lbtdm_app)

# These are disabled by default in the IDF, so switch them back on
CFLAGS += \
	-Werror=unused-function \
	-Werror=unused-but-set-variable \
	-Werror=unused-variable \
