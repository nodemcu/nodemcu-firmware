ifneq ($(CONFIG_LUA_EMBEDDED_FLASH_STORE),0x0)

COMPONENT_OBJS := $(COMPONENT_BUILD_DIR)/luac_out.o
COMPONENT_EXTRA_CLEAN := $(COMPONENT_OBJS) $(COMPONENT_BUILD_DIR)/luac_out.o.bin
COMPONENT_ADD_LINKER_DEPS := $(COMPONENT_BUILD_DIR)/luac_out.o
EMBEDDED_LFS_DATA:= $(BUILD_DIR_BASE)/luac.out

$(COMPONENT_BUILD_DIR)/luac_out.o: $(EMBEDDED_LFS_DATA)
	echo "embedding luac.out into object file $@..."
	dd if=/dev/zero bs=1 count=$$(( $(CONFIG_LUA_EMBEDDED_FLASH_STORE) )) of="$@.bin" status=none
	dd if="$(EMBEDDED_LFS_DATA)" conv=notrunc of="$@.bin" status=none
	cd $(dir $@) && $(OBJCOPY) --input-target binary --output-target elf32-xtensa-le --binary-architecture xtensa --rename-section .data=.lfs.reserved --redefine-sym _binary_luac_out_o_bin_start=lua_flash_store_reserved "$(notdir $@.bin)" "$@"

$(EMBEDDED_LFS_DATA):
	touch $@

endif
