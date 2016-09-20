COMPONENT_ADD_INCLUDEDIRS:=include
# Note: It appears this component must come lexicographically before esp32
# in order to get the -T arguments in the right order.
COMPONENT_ADD_LDFLAGS:=-L $(abspath ld) -T nodemcu_core.ld -lbase_nodemcu

include $(IDF_PATH)/make/component_common.mk
