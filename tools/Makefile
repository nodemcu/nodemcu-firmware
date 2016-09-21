#############################################################
# Options
#

FSSOURCE ?= ../local/fs/
FLASHSIZE ?= 4mb 32mb 8mb
SUBDIRS = 
HOSTCC ?= gcc

OBJDUMP = $(or $(shell which objdump),xtensa-lx106-elf-objdump)

#############################################################
# Get the files to pack into the spiffs image
#

SPIFFSFILES ?= $(patsubst $(FSSOURCE)%,%,$(shell find $(FSSOURCE) -name '*' '!' -name .gitignore ))

#################################################################
# Get the filesize of /bin/0x10000.bin
#

FLASH_USED_END = $$((0x`$(OBJDUMP) -t ../app/.output/eagle/debug/image/eagle.app.v6.out |grep _flash_used_end |cut -f1 -d" "` - 0x40200000))


#############################################################
# Rules base
#
#

all:	spiffsscript

.PHONY: spiffsimg

.PHONY: spiffsimg/spiffsimg

spiffsimg: spiffsimg/spiffsimg
	@echo Built spiffsimg in spiffsimg/spiffsimg

spiffsimg/spiffsimg: 
	@$(MAKE) -C spiffsimg CC=$(HOSTCC)

spiffsscript: remove-image spiffsimg/spiffsimg
	rm -f ./spiffsimg/spiffs.lst
	echo "" >> ./spiffsimg/spiffs.lst
	@$(foreach f, $(SPIFFSFILES), echo "import $(FSSOURCE)$(f) $(f)" >> ./spiffsimg/spiffs.lst ;)
	@$(foreach sz, $(FLASHSIZE), spiffsimg/spiffsimg -U $(FLASH_USED_END) -o ../bin/spiffs-$(sz).dat -f ../bin/0x%x-$(sz).bin -S $(sz) -r ./spiffsimg/spiffs.lst -d; )
	@$(foreach sz, $(FLASHSIZE), if [ -r ../bin/spiffs-$(sz).dat ]; then echo Built $$(cat ../bin/spiffs-$(sz).dat)-$(sz).bin; fi; )
	
remove-image:
	$(foreach sz, $(FLASHSIZE), if [ -r ../bin/spiffs-$(sz).dat ]; then rm -f ../bin/$$(cat ../bin/spiffs-$(sz).dat)-$(sz).bin; fi; )
	rm -f ../bin/spiffs*.dat


spiffsclean: remove-image
	rm -f ./spiffsimg/spiffsimg
	rm -f ./spiffsimg/spiffs.lst

