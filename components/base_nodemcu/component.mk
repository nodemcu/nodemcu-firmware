COMPONENT_ADD_INCLUDEDIRS:=include
# Note: It appears this component must come lexicographically before esp32
# in order to get the -T arguments in the right order.
ifeq ($(CONFIG_LUA_EMBEDDED_FLASH_STORE),0x0)
 NODEMCU_LD_SCRIPT:= nodemcu_core.ld
else
 NODEMCU_LD_SCRIPT:= nodemcu_core_lfs.ld
endif
COMPONENT_ADD_LDFLAGS:=-L $(COMPONENT_PATH)/ld -T $(NODEMCU_LD_SCRIPT) -lbase_nodemcu
