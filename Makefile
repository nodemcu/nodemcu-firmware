# If we haven't got IDF_PATH set already, reinvoke with IDF_PATH in the environ
.NOTPARALLEL:

ifeq ($(IDF_PATH),)
THIS_MK_FILE:=$(notdir $(lastword $(MAKEFILE_LIST)))
THIS_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))
IDF_PATH=$(THIS_DIR)/sdk/esp32-esp-idf
all:
%:
	@echo Setting IDF_PATH and re-invoking...
	@env IDF_PATH=$(IDF_PATH) PATH=$(PATH):$(THIS_DIR)/tools/toolchains/esp32/bin/ $(MAKE) -f $(THIS_MK_FILE) $@

else

PROJECT_NAME:=NodeMCU

# This is, sadly, the cleanest way to resolve the different non-standard
# conventions for sized integers across the various components.
BASIC_TYPES=-Du32_t=uint32_t -Du16_t=uint16_t -Du8_t=uint8_t -Ds32_t=int32_t -Ds16_t=int16_t -Duint32=uint32_t -Duint16=uint16_t -Duint8=uint8_t -Dsint32=int32_t -Dsint16=int16_t -Dsint8=int8_t

include $(IDF_PATH)/make/project.mk

# Ensure these overrides are always used
CC:=$(CC) $(BASIC_TYPES) -D__ESP32__ $(MORE_CFLAGS)

endif
