#  copyright (c) 2010 Espressif System
#
.NOTPARALLEL:
ifndef PDIR

endif

SUBDIRS ?= $(patsubst %/,%,$(dir $(wildcard */Makefile)))

#############################################################
# Functions
#

define ShortcutRule
$(1): .subdirs $(2)/$(1)
endef

SPIFFSFILES ?= $(patsubst ./files/%,%,$(shell find ./files/ -name '*' ))

#############################################################
# Rules base
# Should be done in top-level makefile only
#

all:	.subdirs spiffssript

spiffssript:	
	rm -f spiffs.lst
	$(foreach f, $(SPIFFSFILES), echo "import files/$(f) $(f)" >> spiffs.lst ;)
	spiffsimg/spiffsimg -f spiffs.bin -c 196608 -r spiffs.lst 

clean:
	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clean;)

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
