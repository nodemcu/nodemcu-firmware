APP_DIR = ../../app
summary ?= @true

CC  =gcc

SRCS=\
	main.c \
  $(APP_DIR)/spiffs/spiffs_cache.c  $(APP_DIR)/spiffs/spiffs_check.c  $(APP_DIR)/spiffs/spiffs_gc.c  $(APP_DIR)/spiffs/spiffs_hydrogen.c  $(APP_DIR)/spiffs/spiffs_nucleus.c

CFLAGS=-g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -I. -I$(APP_DIR)/spiffs -I$(APP_DIR)/include -DNODEMCU_SPIFFS_NO_INCLUDE --include spiffs_typedefs.h -Ddbg_printf=printf

spiffsimg: $(SRCS)
	$(summary) HOSTCC $(CURDIR)/$<
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f spiffsimg
