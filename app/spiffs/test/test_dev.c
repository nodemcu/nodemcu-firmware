/*
 * test_dev.c
 *
 *  Created on: Jul 14, 2013
 *      Author: petera
 */


#include "testrunner.h"
#include "test_spiffs.h"
#include "spiffs_nucleus.h"
#include "spiffs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>


SUITE(dev_tests)
void setup() {
  _setup();
}
void teardown() {
  _teardown();
}

TEST(interrupted_write) {
  char *name = "interrupt";
  char *name2 = "interrupt2";
  int res;
  spiffs_file fd;

  const u32_t sz = SPIFFS_CFG_LOG_PAGE_SZ(FS)*8;
  u8_t *buf = malloc(sz);
  memrand(buf, sz);

  printf("  create reference file\n");
  fd = SPIFFS_open(FS, name, SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(fd > 0);
  clear_flash_ops_log();
  res = SPIFFS_write(FS, fd, buf, sz);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  u32_t written = get_flash_ops_log_write_bytes();
  printf("  written bytes: %i\n", written);


  printf("  create error file\n");
  fd = SPIFFS_open(FS, name2, SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(fd > 0);
  clear_flash_ops_log();
  invoke_error_after_write_bytes(written/2, 0);
  res = SPIFFS_write(FS, fd, buf, sz);
  SPIFFS_close(FS, fd);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_TEST);

  clear_flash_ops_log();

#if SPIFFS_CACHE
  // delete all cache
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif


  printf("  read error file\n");
  fd = SPIFFS_open(FS, name2, SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);

  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  printf("  file size: %i\n", s.size);

  if (s.size > 0) {
    u8_t *buf2 = malloc(s.size);
    res = SPIFFS_read(FS, fd, buf2, s.size);
    TEST_CHECK(res >= 0);

    u32_t ix = 0;
    for (ix = 0; ix < s.size; ix += 16) {
      int i;
      printf("  ");
      for (i = 0; i < 16; i++) {
        printf("%02x", buf[ix+i]);
      }
      printf("  ");
      for (i = 0; i < 16; i++) {
        printf("%02x", buf2[ix+i]);
      }
      printf("\n");
    }
    free(buf2);
  }
  SPIFFS_close(FS, fd);


  printf("  FS check\n");
  SPIFFS_check(FS);

  printf("  read error file again\n");
  fd = SPIFFS_open(FS, name2, SPIFFS_APPEND | SPIFFS_RDWR, 0);
  TEST_CHECK(fd > 0);
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  printf("  file size: %i\n", s.size);
  printf("  write file\n");
  res = SPIFFS_write(FS, fd, buf, sz);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  free(buf);

  return TEST_RES_OK;

} TEST_END(interrupted_write)

SUITE_END(dev_tests)
