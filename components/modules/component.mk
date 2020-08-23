# Match up all the module source files with their corresponding Kconfig
# option in the form NODEMCU_CMODULE_<modname> and if enabled, add a
# "-u <modname>_module_selected1" option to force the linker to include
# the module. See components/core/include/module.h for further details on
# how this works.
-include $(PROJECT_PATH)/build/include/config/auto.conf
include $(PROJECT_PATH)/components/modules/uppercase.mk

ifneq (4.0, $(firstword $(sort $(MAKE_VERSION) 4.0)))
  # make versions below 4.0 will fail on the uppercase function used in
  # the exapnsion of MODULE_NAMES.
  $(error GNU make version 4.0 or above required)
endif

MODULE_NAMES:=$(call uppercase,$(patsubst $(COMPONENT_PATH)/%.c,%,$(wildcard $(COMPONENT_PATH)/*.c)))
FORCE_LINK:=$(foreach mod,$(MODULE_NAMES),$(if $(CONFIG_NODEMCU_CMODULE_$(mod)), -u $(mod)_module_selected1))
COMPONENT_ADD_LDFLAGS=$(FORCE_LINK) -lmodules $(if $(CONFIG_NODEMCU_CMODULE_BTHCI),-lbtdm_app)

# These are disabled by default in the IDF, so switch them back on
CFLAGS += \
	-Werror=unused-function \
	-Werror=unused-but-set-variable \
	-Werror=unused-variable \

COMPONENT_EXTRA_CLEAN := u8g2_fonts.h u8g2_displays.h ucg_config.h

u8g2.o: u8g2_fonts.h u8g2_displays.h

u8g2_fonts.h: $(BUILD_DIR_BASE)/include/sdkconfig.h
	perl -w $(PROJECT_PATH)/tools/u8g2_config_fonts.pl < $^ > $@

u8g2_displays.h: $(BUILD_DIR_BASE)/include/sdkconfig.h
	perl -w $(PROJECT_PATH)/tools/u8g2_config_displays.pl < $^ > $@

ucg.o: ucg_config.h

ucg_config.h: $(BUILD_DIR_BASE)/include/sdkconfig.h
	perl -w $(PROJECT_PATH)/tools/ucg_config.pl < $^ > $@
