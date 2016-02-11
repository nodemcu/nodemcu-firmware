#  copyright (c) 2010 Espressif System
#
.NOTPARALLEL:
ifndef PDIR

endif
#############################################################
# Options
#

FSSOURCE = ../local/fs/
FSSIZE = 196608

#############################################################
# recursion
#

SUBDIRS ?= $(patsubst %/,%,$(dir $(wildcard */Makefile)))

#############################################################
# Functions
#

define ShortcutRule
$(1): .subdirs $(2)/$(1)
endef

SPIFFSFILES ?= $(patsubst $(FSSOURCE)%,%,$(shell find $(FSSOURCE) -name '*' ))

#################################################################
# Get the filesize of /bin/0x10000.bin
#

ROM0X10000SIZE = $$(du -b ../bin/0x10000.bin | cut -f 1)

#################################################################
# Calculate the address, the spiffs has o be placed for the given
# 0x10000.bin file size.
#

SPIFFSBASE = $$(( ( ($(ROM0X10000SIZE) + 0x13FFF) / 0x4000) * 0x4000))
SPIFFSBASEHEX = $$( echo "obase=16; $(SPIFFSBASE)" | bc )


#############################################################
# Rules base
#

all:	.subdirs spiffssript

spiffssript:	
	rm -f ./spiffsimg/spiffs.lst
	echo "" >> ./spiffsimg/spiffs.lst
	$(foreach f, $(SPIFFSFILES), echo "import $(FSSOURCE)$(f) $(f)" >> ./spiffsimg/spiffs.lst ;)
	spiffsimg/spiffsimg -f ../bin/0x$(SPIFFSBASEHEX).bin -c $(FSSIZE) -r ./spiffsimg/spiffs.lst 
	

clean:
	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clean;)
	rm -f ./spiffsimg/spiffs.lst

.subdirs:
	@set -e; $(foreach d, $(SUBDIRS), $(MAKE) -C $(d);)

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
