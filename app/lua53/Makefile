
#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of images to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
ifndef PDIR
SUBDIRS = host
GEN_LIBS = liblua.a
endif

STD_CFLAGS=-std=gnu11 -Wimplicit -Wall

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
#
#DEFINES += -DDEVELOPMENT_TOOLS -DDEVELOPMENT_USE_GDB -DDEVELOPMENT_BREAK_ON_STARTUP_PIN=1
#EXTRA_CCFLAGS += -ggdb -O0

#############################################################
# Recursion Magic - Don't touch this!!
#

INCLUDES := $(INCLUDES) -I $(PDIR)include
INCLUDES += -I ./
INCLUDES += -I ../spiffs
INCLUDES += -I ../libc
INCLUDES += -I ../modules
INCLUDES += -I ../platform
INCLUDES += -I ../uzlib
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile
