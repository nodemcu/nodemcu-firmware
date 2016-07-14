#include "c_stdio.h"
#include "platform.h"
#include "spiffs.h"

spiffs fs;

#define LOG_PAGE_SIZE       	256
#define LOG_BLOCK_SIZE		(INTERNAL_FLASH_SECTOR_SIZE * 2)
#define LOG_BLOCK_SIZE_SMALL_FS	(INTERNAL_FLASH_SECTOR_SIZE)
#define MIN_BLOCKS_FS		4
  
static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];
#if SPIFFS_CACHE
static u8_t spiffs_cache[(LOG_PAGE_SIZE+32)*2];
#endif

static s32_t my_spiffs_read(u32_t addr, u32_t size, u8_t *dst) {
  platform_flash_read(dst, addr, size);
  return SPIFFS_OK;
}

static s32_t my_spiffs_write(u32_t addr, u32_t size, u8_t *src) {
  platform_flash_write(src, addr, size);
  return SPIFFS_OK;
}

static s32_t my_spiffs_erase(u32_t addr, u32_t size) {
  u32_t sect_first = platform_flash_get_sector_of_address(addr);
  u32_t sect_last = sect_first;
  while( sect_first <= sect_last )
    if( platform_flash_erase_sector( sect_first ++ ) == PLATFORM_ERR )
      return SPIFFS_ERR_INTERNAL;
  return SPIFFS_OK;
} 

void myspiffs_check_callback(spiffs_check_type type, spiffs_check_report report, u32_t arg1, u32_t arg2){
  // if(SPIFFS_CHECK_PROGRESS == report) return;
  // NODE_ERR("type: %d, report: %d, arg1: %d, arg2: %d\n", type, report, arg1, arg2);
}

/*******************
The W25Q32BV array is organized into 16,384 programmable pages of 256-bytes each. Up to 256 bytes can be programmed at a time. 
Pages can be erased in groups of 16 (4KB sector erase), groups of 128 (32KB block erase), groups of 256 (64KB block erase) or 
the entire chip (chip erase). The W25Q32BV has 1,024 erasable sectors and 64 erasable blocks respectively. 
The small 4KB sectors allow for greater flexibility in applications that require data and parameter storage. 

********************/

static bool myspiffs_set_location(spiffs_config *cfg, int align, int offset, int block_size) {
#ifdef SPIFFS_FIXED_LOCATION
  cfg->phys_addr = (SPIFFS_FIXED_LOCATION + block_size - 1) & ~(block_size-1);
#else
  cfg->phys_addr = ( u32_t )platform_flash_get_first_free_block_address( NULL ) + offset; 
  cfg->phys_addr = (cfg->phys_addr + align - 1) & ~(align - 1);
#endif
#ifdef SPIFFS_SIZE_1M_BOUNDARY
  cfg->phys_size = ((0x100000 - (SYS_PARAM_SEC_NUM * INTERNAL_FLASH_SECTOR_SIZE) - ( ( u32_t )cfg->phys_addr )) & ~(block_size - 1)) & 0xfffff;
#else
  cfg->phys_size = (INTERNAL_FLASH_SIZE - ( ( u32_t )cfg->phys_addr )) & ~(block_size - 1);
#endif
  if ((int) cfg->phys_size < 0) {
    return FALSE;
  }
  cfg->log_block_size = block_size; 

  return (cfg->phys_size / block_size) >= MIN_BLOCKS_FS;
}

/*
 * Returns  TRUE if FS was found
 * align must be a power of two
 */
static bool myspiffs_set_cfg(spiffs_config *cfg, int align, int offset, bool force_create) {
  cfg->phys_erase_block = INTERNAL_FLASH_SECTOR_SIZE; // according to datasheet
  cfg->log_page_size = LOG_PAGE_SIZE; // as we said

  cfg->hal_read_f = my_spiffs_read;
  cfg->hal_write_f = my_spiffs_write;
  cfg->hal_erase_f = my_spiffs_erase;

  if (!myspiffs_set_location(cfg, align, offset, LOG_BLOCK_SIZE)) {
    if (!myspiffs_set_location(cfg, align, offset, LOG_BLOCK_SIZE_SMALL_FS)) {
      return FALSE;
    }
  }

  NODE_DBG("fs.start:%x,max:%x\n",cfg->phys_addr,cfg->phys_size);

#ifdef SPIFFS_USE_MAGIC_LENGTH
  if (force_create) {
    return TRUE;
  }

  int size = SPIFFS_probe_fs(cfg);

  if (size > 0 && size < cfg->phys_size) {
    NODE_DBG("Overriding size:%x\n",size);
    cfg->phys_size = size;
  }
  if (size > 0) {
    return TRUE;
  }
  return FALSE;
#else
  return TRUE;
#endif
}

static bool myspiffs_find_cfg(spiffs_config *cfg, bool force_create) {
  int i;

  if (!force_create) {
#ifdef SPIFFS_FIXED_LOCATION
    if (myspiffs_set_cfg(cfg, 0, 0, FALSE)) {
      return TRUE;
    }
#else
    if (INTERNAL_FLASH_SIZE >= 700000) {
      for (i = 0; i < 8; i++) {
	if (myspiffs_set_cfg(cfg, 0x10000, 0x10000 * i, FALSE)) {
	  return TRUE;
	}
      }
    }

    for (i = 0; i < 8; i++) {
      if (myspiffs_set_cfg(cfg, LOG_BLOCK_SIZE, LOG_BLOCK_SIZE * i, FALSE)) {
	return TRUE;
      }
    }
#endif
  }

  // No existing file system -- set up for a format
  if (INTERNAL_FLASH_SIZE >= 700000) {
    myspiffs_set_cfg(cfg, 0x10000, 0x10000, TRUE);
#ifndef SPIFFS_MAX_FILESYSTEM_SIZE
    if (cfg->phys_size < 400000) {
      // Don't waste so much in alignment
      myspiffs_set_cfg(cfg, LOG_BLOCK_SIZE, LOG_BLOCK_SIZE * 4, TRUE);
    }
#endif
  } else {
    myspiffs_set_cfg(cfg, LOG_BLOCK_SIZE, 0, TRUE);
  }

#ifdef SPIFFS_MAX_FILESYSTEM_SIZE
  if (cfg->phys_size > SPIFFS_MAX_FILESYSTEM_SIZE) {
    cfg->phys_size = (SPIFFS_MAX_FILESYSTEM_SIZE) & ~(cfg->log_block_size - 1);
  }
#endif
  
  return FALSE;
}

static bool myspiffs_mount_internal(bool force_mount) {
  spiffs_config cfg;
  if (!myspiffs_find_cfg(&cfg, force_mount) && !force_mount) {
    return FALSE;
  }

  fs.err_code = 0;

  int res = SPIFFS_mount(&fs,
    &cfg,
    spiffs_work_buf,
    spiffs_fds,
    sizeof(spiffs_fds),
#if SPIFFS_CACHE
    spiffs_cache,
    sizeof(spiffs_cache),
#else
    0, 0,
#endif
    // myspiffs_check_callback);
    0);
  NODE_DBG("mount res: %d, %d\n", res, fs.err_code);
  return res == SPIFFS_OK;
}

bool myspiffs_mount() {
  return myspiffs_mount_internal(FALSE);
}

void myspiffs_unmount() {
  SPIFFS_unmount(&fs);
}

// FS formatting function
// Returns 1 if OK, 0 for error
int myspiffs_format( void )
{
  SPIFFS_unmount(&fs);
  myspiffs_mount_internal(TRUE);
  SPIFFS_unmount(&fs);

  NODE_DBG("Formatting: size 0x%x, addr 0x%x\n", fs.cfg.phys_size, fs.cfg.phys_addr);

  if (SPIFFS_format(&fs) < 0) {
    return 0;
  }

  return myspiffs_mount();
}

int myspiffs_check( void )
{
  // ets_wdt_disable();
  // int res = (int)SPIFFS_check(&fs);
  // ets_wdt_enable();
  // return res;
}

int myspiffs_open(const char *name, int flags){
  return (int)SPIFFS_open(&fs, (char *)name, (spiffs_flags)flags, 0);
}

int myspiffs_close( int fd ){
  return SPIFFS_close(&fs, (spiffs_file)fd);
}
size_t myspiffs_write( int fd, const void* ptr, size_t len ){
#if 0
  if(fd==c_stdout || fd==c_stderr){
    uart0_tx_buffer((u8_t*)ptr, len);
    return len;
  }
#endif
  int res = SPIFFS_write(&fs, (spiffs_file)fd, (void *)ptr, len);
  if (res < 0) {
    NODE_DBG("write errno %i\n", SPIFFS_errno(&fs));
    return 0;
  }
  return res;
}
size_t myspiffs_read( int fd, void* ptr, size_t len){
  int res = SPIFFS_read(&fs, (spiffs_file)fd, ptr, len);
  if (res < 0) {
    NODE_DBG("read errno %i\n", SPIFFS_errno(&fs));
    return 0;
  }
  return res;
}
int myspiffs_lseek( int fd, int off, int whence ){
  return SPIFFS_lseek(&fs, (spiffs_file)fd, off, whence);
}
int myspiffs_eof( int fd ){
  return SPIFFS_eof(&fs, (spiffs_file)fd);
}
int myspiffs_tell( int fd ){
  return SPIFFS_tell(&fs, (spiffs_file)fd);
}
int myspiffs_getc( int fd ){
  unsigned char c = 0xFF;
  int res;
  if(!myspiffs_eof(fd)){
    res = SPIFFS_read(&fs, (spiffs_file)fd, &c, 1);
    if (res != 1) {
      NODE_DBG("getc errno %i\n", SPIFFS_errno(&fs));
      return (int)EOF;
    } else {
      return (int)c;
    }
  }
  return (int)EOF;
}
int myspiffs_ungetc( int c, int fd ){
  return SPIFFS_lseek(&fs, (spiffs_file)fd, -1, SEEK_CUR);
}
int myspiffs_flush( int fd ){
  return SPIFFS_fflush(&fs, (spiffs_file)fd);
}
int myspiffs_error( int fd ){
  return SPIFFS_errno(&fs);
}
void myspiffs_clearerr( int fd ){
  SPIFFS_clearerr(&fs);
}
int myspiffs_rename( const char *old, const char *newname ){
  return SPIFFS_rename(&fs, (char *)old, (char *)newname);
}
size_t myspiffs_size( int fd ){
  int32_t curpos = SPIFFS_tell(&fs, (spiffs_file) fd);
  int32_t size = SPIFFS_lseek(&fs, (spiffs_file) fd, SPIFFS_SEEK_END, 0);
  (void) SPIFFS_lseek(&fs, (spiffs_file) fd, SPIFFS_SEEK_SET, curpos);
  return size;
}
#if 0
void test_spiffs() {
  char buf[12];

  // Surely, I've mounted spiffs before entering here
  
  spiffs_file fd = SPIFFS_open(&fs, "my_file", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);
  if (SPIFFS_write(&fs, fd, (u8_t *)"Hello world", 12) < 0) NODE_DBG("errno %i\n", SPIFFS_errno(&fs));
  SPIFFS_close(&fs, fd); 

  fd = SPIFFS_open(&fs, "my_file", SPIFFS_RDWR, 0);
  if (SPIFFS_read(&fs, fd, (u8_t *)buf, 12) < 0) NODE_DBG("errno %i\n", SPIFFS_errno(&fs));
  SPIFFS_close(&fs, fd);

  NODE_DBG("--> %s <--\n", buf);
}
#endif
