#ifndef _LFS_H_
#define _LFS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  uint32_t    size;
  uint32_t    addr_phys;   // physical address in flash
  const void *addr_mem; // memory mapped address
} lfs_location_info_t;

bool lfs_get_location(lfs_location_info_t *out);

// Setting & getting of filename during reboot/load cycle
bool lfs_get_load_filename(char *buf, size_t bufsiz);
bool lfs_set_load_filename(const char *fname);
bool lfs_clear_load_filename(void);

#endif
