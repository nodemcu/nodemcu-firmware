/*
 * test_spiffs.c
 *
 *  Created on: Jun 19, 2013
 *      Author: petera
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "params_test.h"
#include "spiffs.h"
#include "spiffs_nucleus.h"

#include "testrunner.h"

#include "test_spiffs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

static unsigned char area[PHYS_FLASH_SIZE];

static int erases[256];
static char _path[256];
static u32_t bytes_rd = 0;
static u32_t bytes_wr = 0;
static u32_t reads = 0;
static u32_t writes = 0;
static u32_t error_after_bytes_written = 0;
static u32_t error_after_bytes_read = 0;
static char error_after_bytes_written_once_only = 0;
static char error_after_bytes_read_once_only = 0;
static char log_flash_ops = 1;
static u32_t fs_check_fixes = 0;

spiffs __fs;
static u8_t _work[LOG_PAGE*2];
static u8_t _fds[FD_BUF_SIZE];
static u8_t _cache[CACHE_BUF_SIZE];

static int check_valid_flash = 1;

#define TEST_PATH "test_data/"

char *make_test_fname(const char *name) {
  sprintf(_path, "%s%s", TEST_PATH, name);
  return _path;
}

void clear_test_path() {
  DIR *dp;
  struct dirent *ep;
  dp = opendir(TEST_PATH);

  if (dp != NULL) {
    while ((ep = readdir(dp))) {
      if (ep->d_name[0] != '.') {
        sprintf(_path, "%s%s", TEST_PATH, ep->d_name);
        remove(_path);
      }
    }
    closedir(dp);
  }
}

static s32_t _read(u32_t addr, u32_t size, u8_t *dst) {
  if (log_flash_ops) {
    bytes_rd += size;
    reads++;
    if (error_after_bytes_read > 0 && bytes_rd >= error_after_bytes_read) {
      if (error_after_bytes_read_once_only) {
        error_after_bytes_read = 0;
      }
      return SPIFFS_ERR_TEST;
    }
  }
  if (addr < __fs.cfg.phys_addr) {
    printf("FATAL read addr too low %08x < %08x\n", addr, SPIFFS_PHYS_ADDR);
    exit(0);
  }
  if (addr + size > __fs.cfg.phys_addr + __fs.cfg.phys_size) {
    printf("FATAL read addr too high %08x + %08x > %08x\n", addr, size, SPIFFS_PHYS_ADDR + SPIFFS_FLASH_SIZE);
    exit(0);
  }
  memcpy(dst, &area[addr], size);
  return 0;
}

static s32_t _write(u32_t addr, u32_t size, u8_t *src) {
  int i;
  //printf("wr %08x %i\n", addr, size);
  if (log_flash_ops) {
    bytes_wr += size;
    writes++;
    if (error_after_bytes_written > 0 && bytes_wr >= error_after_bytes_written) {
      if (error_after_bytes_written_once_only) {
        error_after_bytes_written = 0;
      }
      return SPIFFS_ERR_TEST;
    }
  }

  if (addr < __fs.cfg.phys_addr) {
    printf("FATAL write addr too low %08x < %08x\n", addr, SPIFFS_PHYS_ADDR);
    exit(0);
  }
  if (addr + size > __fs.cfg.phys_addr + __fs.cfg.phys_size) {
    printf("FATAL write addr too high %08x + %08x > %08x\n", addr, size, SPIFFS_PHYS_ADDR + SPIFFS_FLASH_SIZE);
    exit(0);
  }

  for (i = 0; i < size; i++) {
    if (((addr + i) & (__fs.cfg.log_page_size-1)) != offsetof(spiffs_page_header, flags)) {
      if (check_valid_flash && ((area[addr + i] ^ src[i]) & src[i])) {
        printf("trying to write %02x to %02x at addr %08x\n", src[i], area[addr + i], addr+i);
        spiffs_page_ix pix = (addr + i) / LOG_PAGE;
        dump_page(&__fs, pix);
        return -1;
      }
    }
    area[addr + i] &= src[i];
  }
  return 0;
}

static s32_t _erase(u32_t addr, u32_t size) {
  if (addr & (__fs.cfg.phys_erase_block-1)) {
    printf("trying to erase at addr %08x, out of boundary\n", addr);
    return -1;
  }
  if (size & (__fs.cfg.phys_erase_block-1)) {
    printf("trying to erase at with size %08x, out of boundary\n", size);
    return -1;
  }
  erases[(addr-__fs.cfg.phys_addr)/__fs.cfg.phys_erase_block]++;
  memset(&area[addr], 0xff, size);
  return 0;
}

void hexdump_mem(u8_t *b, u32_t len) {
  while (len--) {
    if ((((intptr_t)b)&0x1f) == 0) {
      printf("\n");
    }
    printf("%02x", *b++);
  }
  printf("\n");
}

void hexdump(u32_t addr, u32_t len) {
  int remainder = (addr % 32) == 0 ? 0 : 32 - (addr % 32);
  u32_t a;
  for (a = addr - remainder; a < addr+len; a++) {
    if ((a & 0x1f) == 0) {
      if (a != addr) {
        printf("  ");
        int j;
        for (j = 0; j < 32; j++) {
          if (a-32+j < addr)
            printf(" ");
          else {
            printf("%c", (area[a-32+j] < 32 || area[a-32+j] >= 0x7f) ? '.' : area[a-32+j]);
          }
        }
      }
      printf("%s    %08x: ", a<=addr ? "":"\n", a);
    }
    if (a < addr) {
      printf("  ");
    } else {
      printf("%02x", area[a]);
    }
  }
  int j;
  printf("  ");
  for (j = 0; j < 32; j++) {
    if (a-32+j < addr)
      printf(" ");
    else {
      printf("%c", (area[a-32+j] < 32 || area[a-32+j] >= 0x7f) ? '.' : area[a-32+j]);
    }
  }
  printf("\n");
}

void dump_page(spiffs *fs, spiffs_page_ix p) {
  printf("page %04x  ", p);
  u32_t addr = SPIFFS_PAGE_TO_PADDR(fs, p);
  if (p % SPIFFS_PAGES_PER_BLOCK(fs) < SPIFFS_OBJ_LOOKUP_PAGES(fs)) {
    // obj lu page
    printf("OBJ_LU");
  } else {
    u32_t obj_id_addr = SPIFFS_BLOCK_TO_PADDR(fs, SPIFFS_BLOCK_FOR_PAGE(fs , p)) +
        SPIFFS_OBJ_LOOKUP_ENTRY_FOR_PAGE(fs, p) * sizeof(spiffs_obj_id);
    spiffs_obj_id obj_id = *((spiffs_obj_id *)&area[obj_id_addr]);
    // data page
    spiffs_page_header *ph = (spiffs_page_header *)&area[addr];
    printf("DATA %04x:%04x  ", obj_id, ph->span_ix);
    printf("%s", ((ph->flags & SPIFFS_PH_FLAG_FINAL) == 0) ? "FIN " : "fin ");
    printf("%s", ((ph->flags & SPIFFS_PH_FLAG_DELET) == 0) ? "DEL " : "del ");
    printf("%s", ((ph->flags & SPIFFS_PH_FLAG_INDEX) == 0) ? "IDX " : "idx ");
    printf("%s", ((ph->flags & SPIFFS_PH_FLAG_USED) == 0) ? "USD " : "usd ");
    printf("%s  ", ((ph->flags & SPIFFS_PH_FLAG_IXDELE) == 0) ? "IDL " : "idl ");
    if (obj_id & SPIFFS_OBJ_ID_IX_FLAG) {
      // object index
      printf("OBJ_IX");
      if (ph->span_ix == 0) {
        printf("_HDR  ");
        spiffs_page_object_ix_header *oix_hdr = (spiffs_page_object_ix_header *)&area[addr];
        printf("'%s'  %i bytes  type:%02x", oix_hdr->name, oix_hdr->size, oix_hdr->type);
      }
    } else {
      // data page
      printf("CONTENT");
    }
  }
  printf("\n");
  u32_t len = fs->cfg.log_page_size;
  hexdump(addr, len);
}

void area_write(u32_t addr, u8_t *buf, u32_t size) {
  int i;
  for (i = 0; i < size; i++) {
    area[addr + i] = *buf++;
  }
}

void area_read(u32_t addr, u8_t *buf, u32_t size) {
  int i;
  for (i = 0; i < size; i++) {
    *buf++ = area[addr + i];
  }
}

void dump_erase_counts(spiffs *fs) {
  spiffs_block_ix bix;
  printf("  BLOCK     |\n");
  printf("   AGE COUNT|\n");
  for (bix = 0; bix < fs->block_count; bix++) {
    printf("----%3i ----|", bix);
  }
  printf("\n");
  for (bix = 0; bix < fs->block_count; bix++) {
    spiffs_obj_id erase_mark;
    _spiffs_rd(fs, 0, 0, SPIFFS_ERASE_COUNT_PADDR(fs, bix), sizeof(spiffs_obj_id), (u8_t *)&erase_mark);
    if (erases[bix] == 0) {
      printf("            |");
    } else {
      printf("%7i %4i|", (fs->max_erase_count - erase_mark), erases[bix]);
    }
  }
  printf("\n");
}

void dump_flash_access_stats() {
  printf("  RD: %10i reads  %10i bytes %10i avg bytes/read\n", reads, bytes_rd, reads == 0 ? 0 : (bytes_rd / reads));
  printf("  WR: %10i writes %10i bytes %10i avg bytes/write\n", writes, bytes_wr, writes == 0 ? 0 : (bytes_wr / writes));
}


static u32_t old_perc = 999;
static void spiffs_check_cb_f(spiffs_check_type type, spiffs_check_report report,
    u32_t arg1, u32_t arg2) {
/*  if (report == SPIFFS_CHECK_PROGRESS && old_perc != arg1) {
    old_perc = arg1;
    printf("CHECK REPORT: ");
    switch(type) {
    case SPIFFS_CHECK_LOOKUP:
      printf("LU "); break;
    case SPIFFS_CHECK_INDEX:
      printf("IX "); break;
    case SPIFFS_CHECK_PAGE:
      printf("PA "); break;
    }
    printf("%i%%\n", arg1 * 100 / 256);
  }*/
  if (report != SPIFFS_CHECK_PROGRESS) {
    if (report != SPIFFS_CHECK_ERROR) fs_check_fixes++;
    printf("   check: ");
    switch (type) {
    case SPIFFS_CHECK_INDEX:
      printf("INDEX  "); break;
    case SPIFFS_CHECK_LOOKUP:
      printf("LOOKUP "); break;
    case SPIFFS_CHECK_PAGE:
      printf("PAGE   "); break;
    default:
      printf("????   "); break;
    }
    if (report == SPIFFS_CHECK_ERROR) {
      printf("ERROR %i", arg1);
    } else if (report == SPIFFS_CHECK_DELETE_BAD_FILE) {
      printf("DELETE BAD FILE %04x", arg1);
    } else if (report == SPIFFS_CHECK_DELETE_ORPHANED_INDEX) {
      printf("DELETE ORPHANED INDEX %04x", arg1);
    } else if (report == SPIFFS_CHECK_DELETE_PAGE) {
      printf("DELETE PAGE %04x", arg1);
    } else if (report == SPIFFS_CHECK_FIX_INDEX) {
      printf("FIX INDEX %04x:%04x", arg1, arg2);
    } else if (report == SPIFFS_CHECK_FIX_LOOKUP) {
      printf("FIX INDEX %04x:%04x", arg1, arg2);
    } else {
      printf("??");
    }
    printf("\n");
  }
}

void fs_reset_specific(u32_t phys_addr, u32_t phys_size,
    u32_t phys_sector_size,
    u32_t log_block_size, u32_t log_page_size) {
  memset(area, 0xcc, sizeof(area));
  memset(&area[phys_addr], 0xff, phys_size);

  spiffs_config c;
  c.hal_erase_f = _erase;
  c.hal_read_f = _read;
  c.hal_write_f = _write;
  c.log_block_size = log_block_size;
  c.log_page_size = log_page_size;
  c.phys_addr = phys_addr;
  c.phys_erase_block = phys_sector_size;
  c.phys_size = phys_size;

  memset(erases,0,sizeof(erases));
  memset(_cache,0,sizeof(_cache));

  SPIFFS_mount(&__fs, &c, _work, _fds, sizeof(_fds), _cache, sizeof(_cache), spiffs_check_cb_f);

  clear_flash_ops_log();
  log_flash_ops = 1;
  fs_check_fixes = 0;
}

void fs_reset() {
  fs_reset_specific(SPIFFS_PHYS_ADDR, SPIFFS_FLASH_SIZE, SECTOR_SIZE, LOG_BLOCK, LOG_PAGE);
}

void set_flash_ops_log(int enable) {
  log_flash_ops = enable;
}

void clear_flash_ops_log() {
  bytes_rd = 0;
  bytes_wr = 0;
  reads = 0;
  writes = 0;
  error_after_bytes_read = 0;
  error_after_bytes_written = 0;
}

u32_t get_flash_ops_log_read_bytes() {
  return bytes_rd;
}

u32_t get_flash_ops_log_write_bytes() {
  return bytes_wr;
}

void invoke_error_after_read_bytes(u32_t b, char once_only) {
  error_after_bytes_read = b;
  error_after_bytes_read_once_only = once_only;
}
void invoke_error_after_write_bytes(u32_t b, char once_only) {
  error_after_bytes_written = b;
  error_after_bytes_written_once_only = once_only;
}

void fs_set_validate_flashing(int i) {
  check_valid_flash = i;
}

void real_assert(int c, const char *n, const char *file, int l) {
  if (c == 0) {
    printf("ASSERT: %s %s @ %i\n", (n ? n : ""), file, l);
    printf("fs errno:%i\n", __fs.err_code);
    exit(0);
  }
}

int read_and_verify(char *name) {
  s32_t res;
  int fd = SPIFFS_open(&__fs, name, SPIFFS_RDONLY, 0);
  if (fd < 0) {
    printf("  read_and_verify: could not open file %s\n", name);
    return fd;
  }
  return read_and_verify_fd(fd, name);
}

int read_and_verify_fd(spiffs_file fd, char *name) {
  s32_t res;
  int pfd = open(make_test_fname(name), O_RDONLY);
  spiffs_stat s;
  res = SPIFFS_fstat(&__fs, fd, &s);
  if (res < 0) {
    printf("  read_and_verify: could not stat file %s\n", name);
    return res;
  }
  if (s.size == 0) {
    SPIFFS_close(&__fs, fd);
    close(pfd);
    return 0;
  }

  //printf("verifying %s, len %i\n", name, s.size);
  int offs = 0;
  u8_t buf_d[256];
  u8_t buf_v[256];
  while (offs < s.size) {
    int read_len = MIN(s.size - offs, sizeof(buf_d));
    res = SPIFFS_read(&__fs, fd, buf_d, read_len);
    if (res < 0) {
      printf("  read_and_verify: could not read file %s offs:%i len:%i filelen:%i\n", name, offs, read_len, s.size);
      return res;
    }
    int pres = read(pfd, buf_v, read_len);
    (void)pres;
    //printf("reading offs:%i len:%i spiffs_res:%i posix_res:%i\n", offs, read_len, res, pres);
    int i;
    int veri_ok = 1;
    for (i = 0; veri_ok && i < read_len; i++) {
      if (buf_d[i] != buf_v[i]) {
        printf("file verification mismatch @ %i, %02x %c != %02x %c\n", offs+i, buf_d[i], buf_d[i], buf_v[i], buf_v[i]);
        int j = MAX(0, i-16);
        int k = MIN(sizeof(buf_d), i+16);
        k = MIN(s.size-offs, k);
        int l;
        for (l = j; l < k; l++) {
          printf("%c", buf_d[l] > 31 ? buf_d[l] : '.');
        }
        printf("\n");
        for (l = j; l < k; l++) {
          printf("%c", buf_v[l] > 31 ? buf_v[l] : '.');
        }
        printf("\n");
        veri_ok = 0;
      }
    }
    if (!veri_ok) {
      SPIFFS_close(&__fs, fd);
      close(pfd);
      printf("data mismatch\n");
      return -1;
    }

    offs += read_len;
  }

  SPIFFS_close(&__fs, fd);
  close(pfd);

  return 0;
}

static void test_on_stop(test *t) {
  printf("  spiffs errno:%i\n", SPIFFS_errno(&__fs));
#if SPIFFS_TEST_VISUALISATION
  SPIFFS_vis(FS);
#endif

}

void memrand(u8_t *b, int len) {
  int i;
  for (i = 0; i < len; i++) {
    b[i] = rand();
  }
}

int test_create_file(char *name) {
  spiffs_stat s;
  spiffs_file fd;
  int res = SPIFFS_creat(FS, name, 0);
  CHECK_RES(res);
  fd = SPIFFS_open(FS, name, SPIFFS_RDONLY, 0);
  CHECK(fd >= 0);
  res = SPIFFS_fstat(FS, fd, &s);
  CHECK_RES(res);
  CHECK(strcmp((char*)s.name, name) == 0);
  CHECK(s.size == 0);
  SPIFFS_close(FS, fd);
  return 0;
}

int test_create_and_write_file(char *name, int size, int chunk_size) {
  int res;
  spiffs_file fd;
  printf("    create and write %s", name);
  res = test_create_file(name);
  if (res < 0) {
    printf(" failed creation, %i\n",res);
  }
  CHECK(res >= 0);
  fd = SPIFFS_open(FS, name, SPIFFS_APPEND | SPIFFS_RDWR, 0);
  if (res < 0) {
    printf(" failed open, %i\n",res);
  }
  CHECK(fd >= 0);
  int pfd = open(make_test_fname(name), O_APPEND | O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  int offset = 0;
  int mark = 0;
  while (offset < size) {
    int len = MIN(size-offset, chunk_size);
    if (offset > mark) {
      mark += size/16;
      printf(".");
      fflush(stdout);
    }
    u8_t *buf = malloc(len);
    memrand(buf, len);
    res = SPIFFS_write(FS, fd, buf, len);
    write(pfd, buf, len);
    free(buf);
    if (res < 0) {
      printf("\n  error @ offset %i, res %i\n", offset, res);
    }
    offset += len;
    CHECK(res >= 0);
  }
  printf("\n");
  close(pfd);

  spiffs_stat stat;
  res = SPIFFS_fstat(FS, fd, &stat);
  if (res < 0) {
    printf(" failed fstat, %i\n",res);
  }
  CHECK(res >= 0);
  if (stat.size != size) {
    printf(" failed size, %i != %i\n", stat.size, size);
  }
  CHECK(stat.size == size);

  SPIFFS_close(FS, fd);
  return 0;
}

#if SPIFFS_CACHE
#if SPIFFS_CACHE_STATS
static u32_t chits_tot = 0;
static u32_t cmiss_tot = 0;
#endif
#endif

void _setup_test_only() {
  fs_set_validate_flashing(1);
  test_init(test_on_stop);
}

void _setup() {
  fs_reset();
  _setup_test_only();
}

void _teardown() {
  printf("  free blocks     : %i of %i\n", (FS)->free_blocks, (FS)->block_count);
  printf("  pages allocated : %i\n", (FS)->stats_p_allocated);
  printf("  pages deleted   : %i\n", (FS)->stats_p_deleted);
#if SPIFFS_GC_STATS
  printf("  gc runs         : %i\n", (FS)->stats_gc_runs);
#endif
#if SPIFFS_CACHE
#if SPIFFS_CACHE_STATS
  chits_tot += (FS)->cache_hits;
  cmiss_tot += (FS)->cache_misses;
  printf("  cache hits      : %i (sum %i)\n", (FS)->cache_hits, chits_tot);
  printf("  cache misses    : %i (sum %i)\n", (FS)->cache_misses, cmiss_tot);
  printf("  cache utiliz    : %f\n", ((float)chits_tot/(float)(chits_tot + cmiss_tot)));
#endif
#endif
  dump_flash_access_stats();
  clear_flash_ops_log();
#if SPIFFS_GC_STATS
  if ((FS)->stats_gc_runs > 0)
#endif
  dump_erase_counts(FS);
  printf("  fs consistency check:\n");
  SPIFFS_check(FS);
  clear_test_path();

  //hexdump_mem(&area[SPIFFS_PHYS_ADDR - 16], 32);
  //hexdump_mem(&area[SPIFFS_PHYS_ADDR + SPIFFS_FLASH_SIZE - 16], 32);
}

u32_t tfile_get_size(tfile_size s) {
  switch (s) {
  case EMPTY:
    return 0;
  case SMALL:
    return SPIFFS_DATA_PAGE_SIZE(FS)/2;
  case MEDIUM:
    return SPIFFS_DATA_PAGE_SIZE(FS) * (SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS));
  case LARGE:
    return (FS)->cfg.phys_size/3;
  }
  return 0;
}

int run_file_config(int cfg_count, tfile_conf* cfgs, int max_runs, int max_concurrent_files, int dbg) {
  int res;
  tfile *tfiles = malloc(sizeof(tfile) * max_concurrent_files);
  memset(tfiles, 0, sizeof(tfile) * max_concurrent_files);
  int run = 0;
  int cur_config_ix = 0;
  char name[32];
  while (run < max_runs)  {
    if (dbg) printf(" run %i/%i\n", run, max_runs);
    int i;
    for (i = 0; i < max_concurrent_files; i++) {
      sprintf(name, "file%i_%i", (1+run), i);
      tfile *tf = &tfiles[i];
      if (tf->state == 0 && cur_config_ix < cfg_count) {
// create a new file
        strcpy(tf->name, name);
        tf->state = 1;
        tf->cfg = cfgs[cur_config_ix];
        int size = tfile_get_size(tf->cfg.tsize);
        if (dbg) printf("   create new %s with cfg %i/%i, size %i\n", name, (1+cur_config_ix), cfg_count, size);

        if (tf->cfg.tsize == EMPTY) {
          res = SPIFFS_creat(FS, name, 0);
          CHECK_RES(res);
          int pfd = open(make_test_fname(name), O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
          close(pfd);
          int extra_flags = tf->cfg.ttype == APPENDED ? SPIFFS_APPEND : 0;
          spiffs_file fd = SPIFFS_open(FS, name, extra_flags | SPIFFS_RDWR, 0);
          CHECK(fd > 0);
          tf->fd = fd;
        } else {
          int extra_flags = tf->cfg.ttype == APPENDED ? SPIFFS_APPEND : 0;
          spiffs_file fd = SPIFFS_open(FS, name, extra_flags | SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR, 0);
          CHECK(fd > 0);
          extra_flags = tf->cfg.ttype == APPENDED ? O_APPEND : 0;
          int pfd = open(make_test_fname(name), extra_flags | O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
          tf->fd = fd;
          u8_t *buf = malloc(size);
          memrand(buf, size);
          res = SPIFFS_write(FS, fd, buf, size);
          CHECK_RES(res);
          write(pfd, buf, size);
          close(pfd);
          free(buf);
          res = read_and_verify(name);
          CHECK_RES(res);
        }

        cur_config_ix++;
      } else if (tf->state > 0) {
// hande file lifecycle
        switch (tf->cfg.ttype) {
        case UNTAMPERED: {
          break;
        }
        case APPENDED: {
          if (dbg) printf("   appending %s\n", tf->name);
          int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
          u8_t *buf = malloc(size);
          memrand(buf, size);
          res = SPIFFS_write(FS, tf->fd, buf, size);
          CHECK_RES(res);
          int pfd = open(make_test_fname(tf->name), O_APPEND | O_RDWR);
          write(pfd, buf, size);
          close(pfd);
          free(buf);
          res = read_and_verify(tf->name);
          CHECK_RES(res);
          break;
        }
        case MODIFIED: {
          if (dbg) printf("   modify %s\n", tf->name);
          spiffs_stat stat;
          res = SPIFFS_fstat(FS, tf->fd, &stat);
          CHECK_RES(res);
          int size = stat.size / tf->cfg.tlife + SPIFFS_DATA_PAGE_SIZE(FS)/3;
          int offs = (stat.size / tf->cfg.tlife) * tf->state;
          res = SPIFFS_lseek(FS, tf->fd, offs, SPIFFS_SEEK_SET);
          CHECK_RES(res);
          u8_t *buf = malloc(size);
          memrand(buf, size);
          res = SPIFFS_write(FS, tf->fd, buf, size);
          CHECK_RES(res);
          int pfd = open(make_test_fname(tf->name), O_RDWR);
          lseek(pfd, offs, SEEK_SET);
          write(pfd, buf, size);
          close(pfd);
          free(buf);
          res = read_and_verify(tf->name);
          CHECK_RES(res);
          break;
        }
        case REWRITTEN: {
          if (tf->fd > 0) {
            SPIFFS_close(FS, tf->fd);
          }
          if (dbg) printf("   rewriting %s\n", tf->name);
          spiffs_file fd = SPIFFS_open(FS, tf->name, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR, 0);
          CHECK(fd > 0);
          int pfd = open(make_test_fname(tf->name), O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
          tf->fd = fd;
          int size = tfile_get_size(tf->cfg.tsize);
          u8_t *buf = malloc(size);
          memrand(buf, size);
          res = SPIFFS_write(FS, fd, buf, size);
          CHECK_RES(res);
          write(pfd, buf, size);
          close(pfd);
          free(buf);
          res = read_and_verify(tf->name);
          CHECK_RES(res);
          break;
        }
        }
        tf->state++;
        if (tf->state > tf->cfg.tlife) {
// file outlived its time, kill it
          if (tf->fd > 0) {
            SPIFFS_close(FS, tf->fd);
          }
          if (dbg) printf("   removing %s\n", tf->name);
          res = read_and_verify(tf->name);
          CHECK_RES(res);
          res = SPIFFS_remove(FS, tf->name);
          CHECK_RES(res);
          remove(make_test_fname(tf->name));
          memset(tf, 0, sizeof(tf));
        }

      }
    }

    run++;
  }
  free(tfiles);
  return 0;
}



