#  copyright (c) 2010 Espressif System
#
.NOTPARALLEL:
ifndef PDIR

endif
#############################################################
# Options
#

FSSOURCE ?= ../local/fs/
FLASHSIZE ?= 4mb 32mb
SUBDIRS = 

#############################################################
# Get the files to pack into the spiffs image
#

SPIFFSFILES ?= $(patsubst $(FSSOURCE)%,%,$(shell find $(FSSOURCE) -name '*' '!' -name .gitignore ))

#################################################################
# Get the filesize of /bin/0x10000.bin
#

ROM0X10000SIZE = $$(du -b ../bin/0x10000.bin | cut -f 1)

#################################################################
# Calculate the address, the spiffs has to be placed for the given
# 0x10000.bin file size.
#

SPIFFSBASE = $$(( ($(ROM0X10000SIZE) + 0x10000) ))


#############################################################
# Rules base
#
#

all:	spiffsscript

spiffsscript: remove-image
	$(MAKE) -C spiffsimg CC=gcc
	rm -f ./spiffsimg/spiffs.lst
	echo "" >> ./spiffsimg/spiffs.lst
	$(foreach f, $(SPIFFSFILES), echo "import $(FSSOURCE)$(f) $(f)" >> ./spiffsimg/spiffs.lst ;)
	$(foreach sz, $(FLASHSIZE), spiffsimg/spiffsimg -U $(SPIFFSBASE) -o ../bin/spiffs-$(sz).dat -f ../bin/0x%x-$(sz).bin -S $(sz) -r ./spiffsimg/spiffs.lst -d; )
	
remove-image:
	$(foreach sz, $(FLASHSIZE), if [ -r ../bin/spiffs-$(sz).dat ]; then rm -f ../bin/$$(cat ../bin/spiffs-$(sz).dat)-$(sz).bin; fi; )
	rm -f ../bin/spiffs*.dat


clean: remove-image
	rm -f ./spiffsimg/spiffsimg
	rm -f ./spiffsimg/spiffs.lst

#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

INCLUDES := $(INCLUDES) -I $(PDIR)include -I $(PDIR)include/$(TARGET)
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile
