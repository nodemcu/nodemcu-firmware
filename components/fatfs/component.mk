COMPONENT_SRCDIRS:=. option
# TODO: add Kconfig to select code page used?
COMPONENT_OBJS:=diskio.o ff.o myfatfs.o option/unicode.o 
COMPONENT_ADD_INCLUDEDIRS:=.

CFLAGS+=-imacros fatfs_prefix_lib.h
