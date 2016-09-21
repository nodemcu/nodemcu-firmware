COMPONENT_ADD_INCLUDEDIRS:=.

# TODO: clean up codebase to be sign clean...
EXTRA_CFLAGS+=-Wno-error=pointer-sign

include $(IDF_PATH)/make/component_common.mk
