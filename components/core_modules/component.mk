COMPONENT_ADD_INCLUDEDIRS:=include
COMPONENT_ADD_LDFLAGS:=-L $(abspath ld) -T core_modules.ld -lcore_modules

include $(IDF_PATH)/make/component_common.mk
