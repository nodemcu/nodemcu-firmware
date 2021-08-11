SHELL:=/bin/bash

ifeq ($(IDF_PATH),)

THIS_MK_FILE:=$(notdir $(lastword $(MAKEFILE_LIST)))
THIS_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))
IDF_PATH=$(THIS_DIR)/sdk/esp32-esp-idf

all:
	. $(IDF_PATH)/export.sh && $(MAKE) "$@"

%:
	. $(IDF_PATH)/export.sh && $(MAKE) "$@"

else

all:
	$(IDF_PATH)/tools/idf.py $(IDFPY_ARGS) "$@"

%:
	$(IDF_PATH)/tools/idf.py $(IDFPY_ARGS) "$@"

endif

# FIXME - needs updating to work in IDF4
#
#extmod-update:
#	@tools/extmod/extmod.sh update
#
#extmod-clean:
##	@tools/extmod/extmod.sh clean
