#############################################################
# Options
#

FSSOURCE ?= ../local/fs
LUASOURCE ?= ../local/lua
FLASHSIZE ?= 4mb 32mb 8mb
FLASH_SW = -S
SUBDIRS = 

OBJDUMP = $(or $(shell which objdump),xtensa-lx106-elf-objdump)

#############################################################
# Get the files to pack into the spiffs image
#

SPIFFSFILES ?= $(patsubst $(FSSOURCE)/%,%,$(shell find $(FSSOURCE)/ -name '*' '!' -name .gitignore ))

#################################################################
# Get the filesize of /bin/0x10000.bin and SPIFFS sizing
#

FLASH_FS_SIZE := $(shell $(CC) -E -dM - <../app/include/user_config.h | grep SPIFFS_MAX_FILESYSTEM_SIZE| cut -d ' ' -f 3)

ifneq ($(strip $(FLASH_FS_SIZE)),)
FLASHSIZE = $(shell printf "0x%x" $(FLASH_FS_SIZE))
FLASH_SW = -c
endif 

FLASH_FS_LOC := $(shell $(CC) -E -dM - <../app/include/user_config.h | grep SPIFFS_FIXED_LOCATION| cut -d ' ' -f 3)
ifeq ($(strip $(FLASH_FS_LOC)),)
FLASH_FS_LOC := $(shell printf "0x%x" $$((0x$(shell $(OBJDUMP) -t ../app/.output/eagle/debug/image/eagle.app.v6.out |grep " _flash_used_end" |cut -f1 -d" ") - 0x40200000)))
else 
FLASH_FS_LOC := $(shell printf "0x%x" $(FLASH_FS_LOC))
endif 

LFSSOURCES := $(wildcard $(LUASOURCE)/*.lua)

BUILD_TYPE := $(shell $(CC) $(EXTRA_CCFLAGS) -E -dM - <../app/include/user_config.h | grep LUA_NUMBER_INTEGRAL | wc -l)
ifeq ($(BUILD_TYPE),0)
LUAC_CROSS := ../luac.cross
else
LUAC_CROSS := ../luac.cross.int
endif

#############################################################
# Rules base
#
#

all:	spiffsscript

.PHONY: TEST

TEST:
	@echo $(FLASHSIZE)
	@echo $(FLASH_FS_SIZE)
	@echo $(FLASH_FS_LOC)
	@echo $(FLASH_USED_END)

spiffsimg/spiffsimg: 

.PHONY: spiffsimg

.PHONY: spiffsimg/spiffsimg

spiffsimg: spiffsimg/spiffsimg
	@echo Built spiffsimg in spiffsimg/spiffsimg

spiffsimg/spiffsimg: 
	@$(MAKE) -C spiffsimg 

spiffsscript: remove-image LFSimage spiffsimg/spiffsimg
	rm -f ./spiffsimg/spiffs.lst
	@echo "" >> ./spiffsimg/spiffs.lst
	@$(foreach f, $(SPIFFSFILES), echo "import $(FSSOURCE)/$(f) $(f)" >> ./spiffsimg/spiffs.lst ;)
	$(foreach sz, $(FLASHSIZE), spiffsimg/spiffsimg -f ../bin/0x%x-$(sz).img  $(FLASH_SW) $(sz) -U $(FLASH_FS_LOC) -r ./spiffsimg/spiffs.lst -d; )
	@$(foreach sz, $(FLASHSIZE), if [ -r ../bin/spiffs-$(sz).dat ]; then echo Built $$(cat ../bin/spiffs-$(sz).dat)-$(sz).bin; fi; )

ifneq ($(LFSSOURCES),)
LFSimage: $(LFSSOURCES)
	$(LUAC_CROSS) -f -o $(FSSOURCE)/LFS.img $(LFSSOURCES)
else
LFSimage: 
	rm -f $(FSSOURCE)/LFS.img
endif

remove-image:
	$(foreach sz, $(FLASHSIZE), if [ -r ../bin/spiffs-$(sz).dat ]; then rm -f ../bin/$$(cat ../bin/spiffs-$(sz).dat)-$(sz).bin; fi; )
	rm -f ../bin/spiffs*.dat


spiffsclean: remove-image
	rm -f ./spiffsimg/spiffsimg
	rm -f ./spiffsimg/spiffs.lst

