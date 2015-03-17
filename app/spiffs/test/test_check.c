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


SUITE(check_tests)
void setup() {
  _setup();
}
void teardown() {
  _teardown();
}

TEST(evil_write) {
  fs_set_validate_flashing(0);
  printf("writing corruption to block 1 data range (leaving lu intact)\n");
  u32_t data_range = SPIFFS_CFG_LOG_BLOCK_SZ(FS) -
      SPIFFS_CFG_LOG_PAGE_SZ(FS) * (SPIFFS_OBJ_LOOKUP_PAGES(FS));
  u8_t *corruption = malloc(data_range);
  memrand(corruption, data_range);
  u32_t addr = 0 * SPIFFS_CFG_LOG_PAGE_SZ(FS) * SPIFFS_OBJ_LOOKUP_PAGES(FS);
  area_write(addr, corruption, data_range);
  free(corruption);

  int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
  int res = test_create_and_write_file("file", size, size);

  printf("CHECK1-----------------\n");
  SPIFFS_check(FS);
  printf("CHECK2-----------------\n");
  SPIFFS_check(FS);
  printf("CHECK3-----------------\n");
  SPIFFS_check(FS);

  res = test_create_and_write_file("file2", size, size);
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
} TEST_END(evil_write)


TEST(lu_check1) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify lu entry data page index 1
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id & ~SPIFFS_OBJ_ID_IX_FLAG, 1, 0, &pix);
  TEST_CHECK(res >= 0);

  // reset lu entry to being erased, but keep page data
  spiffs_obj_id obj_id = SPIFFS_OBJ_ID_DELETED;
  spiffs_block_ix bix = SPIFFS_BLOCK_FOR_PAGE(FS, pix);
  int entry = SPIFFS_OBJ_LOOKUP_ENTRY_FOR_PAGE(FS, pix);
  u32_t addr = SPIFFS_BLOCK_TO_PADDR(FS, bix) + entry*sizeof(spiffs_obj_id);

  area_write(addr, (u8_t*)&obj_id, sizeof(spiffs_obj_id));

#if SPIFFS_CACHE
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif
  SPIFFS_check(FS);

  return TEST_RES_OK;
} TEST_END(lu_check1)


TEST(page_cons1) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify object index, find object index header
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id | SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &pix);
  TEST_CHECK(res >= 0);

  // set object index entry 2 to a bad page
  u32_t addr = SPIFFS_PAGE_TO_PADDR(FS, pix) + sizeof(spiffs_page_object_ix_header) + 0 * sizeof(spiffs_page_ix);
  spiffs_page_ix bad_pix_ref = 0x55;
  area_write(addr, (u8_t*)&bad_pix_ref, sizeof(spiffs_page_ix));
  area_write(addr+2, (u8_t*)&bad_pix_ref, sizeof(spiffs_page_ix));

  // delete all cache
#if SPIFFS_CACHE
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif

  SPIFFS_check(FS);

  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
} TEST_END(page_cons1)


TEST(page_cons2) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify object index, find object index header
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id | SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &pix);
  TEST_CHECK(res >= 0);

  // find data page span index 0
  spiffs_page_ix dpix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id & ~SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &dpix);
  TEST_CHECK(res >= 0);

  // set object index entry 1+2 to a data page 0
  u32_t addr = SPIFFS_PAGE_TO_PADDR(FS, pix) + sizeof(spiffs_page_object_ix_header) + 1 * sizeof(spiffs_page_ix);
  spiffs_page_ix bad_pix_ref = dpix;
  area_write(addr, (u8_t*)&bad_pix_ref, sizeof(spiffs_page_ix));
  area_write(addr+sizeof(spiffs_page_ix), (u8_t*)&bad_pix_ref, sizeof(spiffs_page_ix));

  // delete all cache
#if SPIFFS_CACHE
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif

  SPIFFS_check(FS);

  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
} TEST_END(page_cons2)



TEST(page_cons3) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify object index, find object index header
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id | SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &pix);
  TEST_CHECK(res >= 0);

  // set object index entry 1+2 lookup page
  u32_t addr = SPIFFS_PAGE_TO_PADDR(FS, pix) + sizeof(spiffs_page_object_ix_header) + 1 * sizeof(spiffs_page_ix);
  spiffs_page_ix bad_pix_ref = SPIFFS_PAGES_PER_BLOCK(FS) * (*FS.block_count - 2);
  area_write(addr, (u8_t*)&bad_pix_ref, sizeof(spiffs_page_ix));
  area_write(addr+sizeof(spiffs_page_ix), (u8_t*)&bad_pix_ref, sizeof(spiffs_page_ix));

  // delete all cache
#if SPIFFS_CACHE
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif

  SPIFFS_check(FS);

  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
} TEST_END(page_cons3)


TEST(page_cons_final) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify page header, make unfinalized
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id & ~SPIFFS_OBJ_ID_IX_FLAG, 1, 0, &pix);
  TEST_CHECK(res >= 0);

  // set page span ix 1 as unfinalized
  u32_t addr = SPIFFS_PAGE_TO_PADDR(FS, pix) + offsetof(spiffs_page_header, flags);
  u8_t flags;
  area_read(addr, (u8_t*)&flags, 1);
  flags |= SPIFFS_PH_FLAG_FINAL;
  area_write(addr, (u8_t*)&flags, 1);

  // delete all cache
#if SPIFFS_CACHE
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif

  SPIFFS_check(FS);

  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
} TEST_END(page_cons_final)


TEST(index_cons1) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*SPIFFS_PAGES_PER_BLOCK(FS);
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify lu entry data page index header
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id | SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &pix);
  TEST_CHECK(res >= 0);

  printf("  deleting lu entry pix %04x\n", pix);
  // reset lu entry to being erased, but keep page data
  spiffs_obj_id obj_id = SPIFFS_OBJ_ID_DELETED;
  spiffs_block_ix bix = SPIFFS_BLOCK_FOR_PAGE(FS, pix);
  int entry = SPIFFS_OBJ_LOOKUP_ENTRY_FOR_PAGE(FS, pix);
  u32_t addr = SPIFFS_BLOCK_TO_PADDR(FS, bix) + entry * sizeof(spiffs_obj_id);

  area_write(addr, (u8_t*)&obj_id, sizeof(spiffs_obj_id));

#if SPIFFS_CACHE
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif
  SPIFFS_check(FS);

  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
} TEST_END(index_cons1)


TEST(index_cons2) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*SPIFFS_PAGES_PER_BLOCK(FS);
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify lu entry data page index header
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id | SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &pix);
  TEST_CHECK(res >= 0);

  printf("  writing lu entry for index page, ix %04x, as data page\n", pix);
  spiffs_obj_id obj_id = 0x1234;
  spiffs_block_ix bix = SPIFFS_BLOCK_FOR_PAGE(FS, pix);
  int entry = SPIFFS_OBJ_LOOKUP_ENTRY_FOR_PAGE(FS, pix);
  u32_t addr = SPIFFS_BLOCK_TO_PADDR(FS, bix) + entry * sizeof(spiffs_obj_id);

  area_write(addr, (u8_t*)&obj_id, sizeof(spiffs_obj_id));

#if SPIFFS_CACHE
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif
  SPIFFS_check(FS);

  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
} TEST_END(index_cons2)


TEST(index_cons3) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*SPIFFS_PAGES_PER_BLOCK(FS);
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify lu entry data page index header
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id | SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &pix);
  TEST_CHECK(res >= 0);

  printf("  setting lu entry pix %04x to another index page\n", pix);
  // reset lu entry to being erased, but keep page data
  spiffs_obj_id obj_id = 1234 | SPIFFS_OBJ_ID_IX_FLAG;
  spiffs_block_ix bix = SPIFFS_BLOCK_FOR_PAGE(FS, pix);
  int entry = SPIFFS_OBJ_LOOKUP_ENTRY_FOR_PAGE(FS, pix);
  u32_t addr = SPIFFS_BLOCK_TO_PADDR(FS, bix) + entry * sizeof(spiffs_obj_id);

  area_write(addr, (u8_t*)&obj_id, sizeof(spiffs_obj_id));

#if SPIFFS_CACHE
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif
  SPIFFS_check(FS);

  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
} TEST_END(index_cons3)

TEST(index_cons4) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*SPIFFS_PAGES_PER_BLOCK(FS);
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify lu entry data page index header, flags
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id | SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &pix);
  TEST_CHECK(res >= 0);

  printf("  cue objix hdr deletion in page %04x\n", pix);
  // set flags as deleting ix header
  u32_t addr = SPIFFS_PAGE_TO_PADDR(FS, pix) + offsetof(spiffs_page_header, flags);
  u8_t flags = 0xff & ~(SPIFFS_PH_FLAG_FINAL | SPIFFS_PH_FLAG_USED | SPIFFS_PH_FLAG_INDEX | SPIFFS_PH_FLAG_IXDELE);

  area_write(addr, (u8_t*)&flags, 1);

#if SPIFFS_CACHE
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif
  SPIFFS_check(FS);

  return TEST_RES_OK;
} TEST_END(index_cons4)



SUITE_END(check_tests)
