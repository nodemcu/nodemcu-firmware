ifneq ($(CONFIG_LUA_EMBEDDED_FLASH_STORE),0x0)

COMPONENT_OBJS := $(COMPONENT_BUILD_DIR)/luac_out.o
COMPONENT_ADD_LINKER_DEPS := $(COMPONENT_BUILD_DIR)/luac_out.o

$(COMPONENT_BUILD_DIR)/luac_out.o:
	dd if=/dev/zero bs=1 count=$$(( $(CONFIG_LUA_EMBEDDED_FLASH_STORE) )) of="$@.bin" status=none
	[ -e "$(BUILD_DIR_BASE)/luac.out" ] && dd if="$(BUILD_DIR_BASE)/luac.out" conv=notrunc of="$@.bin" status=none
	cd $(dir $@) && $(OBJCOPY) --input-target binary --output-target elf32-xtensa-le --binary-architecture xtensa --rename-section .data=.lfs.reserved --redefine-sym _binary_luac_out_o_bin_start=lua_flash_store_reserved "$(notdir $@.bin)" "$@"

endif
