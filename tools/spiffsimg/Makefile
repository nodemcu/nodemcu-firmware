CC  =gcc

SRCS=\
	main.c \
  ../../app/spiffs/spiffs_cache.c  ../../app/spiffs/spiffs_check.c  ../../app/spiffs/spiffs_gc.c  ../../app/spiffs/spiffs_hydrogen.c  ../../app/spiffs/spiffs_nucleus.c

CFLAGS=-g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -I. -I../../app/spiffs -I../../app/include -DNODEMCU_SPIFFS_NO_INCLUDE --include spiffs_typedefs.h -Ddbg_printf=printf

spiffsimg: $(SRCS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f spiffsimg
