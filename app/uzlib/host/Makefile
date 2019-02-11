#
# This Make file is called from the core Makefile hierarchy with is a hierarchical
# make wwhich uses parent callbacks to implement inheritance.  However is luac_cross
# build stands outside this and uses the host toolchain to implement a separate
# host build of the luac.cross image.
#
.NOTPARALLEL:

summary ?= @true

CCFLAGS:= -I..
LDFLAGS:= -L$(SDK_DIR)/lib -L$(SDK_DIR)/ld -lm -ldl -Wl,-Map=mapfile

CCFLAGS += -Wall

#DEFINES +=

TARGET = host

ifeq ($(FLAVOR),debug)
    CCFLAGS        += -O0 -g
    TARGET_LDFLAGS += -O0 -g
    DEFINES        += -DDEBUG_COUNTS
else
    FLAVOR         =  release
    CCFLAGS        += -O2
    TARGET_LDFLAGS += -O2
endif

#
# This relies on the files being unique on the vpath
#
SRC := uz_unzip.c  uz_zip.c  crc32.c uzlib_inflate.c uzlib_deflate.c
vpath %.c .:..

ODIR   := .output/$(TARGET)/$(FLAVOR)/obj

CFLAGS = $(CCFLAGS) $(DEFINES)  $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)
DFLAGS = $(CCFLAGS) $(DDEFINES) $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)

ROOT = ../../..
CC := $(WRAPCC) gcc

ECHO := echo

IMAGES :=  $(ROOT)/uz_zip $(ROOT)/uz_unzip
.PHONY: test clean all

all: $(IMAGES)

$(ROOT)/uz_zip : $(ODIR)/uz_zip.o $(ODIR)/crc32.o $(ODIR)/uzlib_deflate.o
	$(summary) HOSTLD $@
	$(CC) $^ -o $@ $(LDFLAGS)

$(ROOT)/uz_unzip : $(ODIR)/uz_unzip.o $(ODIR)/crc32.o $(ODIR)/uzlib_inflate.o
	$(summary) HOSTLD $@
	$(CC) $^ -o $@ $(LDFLAGS)

test :
	@echo CC: $(CC)
	@echo SRC: $(SRC)
	@echo DEPS: $(DEPS)

clean :
	$(RM) -r $(ODIR)
	$(RM) $(IMAGES)

$(ODIR)/%.o: %.c
	@mkdir -p $(ODIR);
	$(summary) HOSTCC $(CURDIR)/$<
	$(CC) $(CFLAGS) -o $@ -c $<

