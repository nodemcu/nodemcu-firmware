#include "c_stdio.h"
#include "platform.h"
#include "spiffs.h"
  
spiffs fs;

#define LOG_PAGE_SIZE       256
  
static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];
static u8_t spiffs_cache[(LOG_PAGE_SIZE+32)*4];

static s32_t ICACHE_FLASH_ATTR my_spiffs_read(u32_t addr, u32_t size, u8_t *dst) {
  platform_flash_read(dst, addr, size);
  return SPIFFS_OK;
}

static s32_t ICACHE_FLASH_ATTR my_spiffs_write(u32_t addr, u32_t size, u8_t *src) {
  platform_flash_write(src, addr, size);
  return SPIFFS_OK;
}

static s32_t ICACHE_FLASH_ATTR my_spiffs_erase(u32_t addr, u32_t size) {
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

void ICACHE_FLASH_ATTR spiffs_mount() {
  spiffs_config cfg;
  cfg.phys_addr = ( u32_t )platform_flash_get_first_free_block_address( NULL ); 
  cfg.phys_size = INTERNAL_FLASH_SIZE - ( ( u32_t )cfg.phys_addr - INTERNAL_FLASH_START_ADDRESS );
  cfg.phys_erase_block = INTERNAL_FLASH_SECTOR_SIZE; // according to datasheet
  cfg.log_block_size = INTERNAL_FLASH_SECTOR_SIZE; // let us not complicate things
  cfg.log_page_size = LOG_PAGE_SIZE; // as we said
  NODE_DBG("fs.start:%x,max:%x\n",cfg.phys_addr,cfg.phys_size);

  cfg.hal_read_f = my_spiffs_read;
  cfg.hal_write_f = my_spiffs_write;
  cfg.hal_erase_f = my_spiffs_erase;
  
  int res = SPIFFS_mount(&fs,
    &cfg,
    spiffs_work_buf,
    spiffs_fds,
    sizeof(spiffs_fds),
    spiffs_cache,
    sizeof(spiffs_cache),
    // myspiffs_check_callback);
    0);
  NODE_DBG("mount res: %i\n", res);
}

// FS formatting function
// Returns 1 if OK, 0 for error
int ICACHE_FLASH_ATTR myspiffs_format( void )
{
  SPIFFS_unmount(&fs);
  u32_t sect_first, sect_last;
  sect_first = ( u32_t )platform_flash_get_first_free_block_address( NULL ); 
  sect_first = platform_flash_get_sector_of_address(sect_first);
  sect_last = INTERNAL_FLASH_SIZE + INTERNAL_FLASH_START_ADDRESS - 4;
  sect_last = platform_flash_get_sector_of_address(sect_last);
  NODE_DBG("sect_first: %x, sect_last: %x\n", sect_first, sect_last);
  while( sect_first <= sect_last )
    if( platform_flash_erase_sector( sect_first ++ ) == PLATFORM_ERR )
      return 0;
  return 1;
}

int ICACHE_FLASH_ATTR myspiffs_check( void )
{
  // ets_wdt_disable();
  // int res = (int)SPIFFS_check(&fs);
  // ets_wdt_enable();
  // return res;
}

int ICACHE_FLASH_ATTR myspiffs_open(const char *name, int flags){
  return (int)SPIFFS_open(&fs, name, (spiffs_flags)flags, 0);
}

int ICACHE_FLASH_ATTR myspiffs_close( int fd ){
  SPIFFS_close(&fs, (spiffs_file)fd);
  return 0;
}
size_t ICACHE_FLASH_ATTR myspiffs_write( int fd, const void* ptr, size_t len ){
#if 0
  if(fd==c_stdout || fd==c_stderr){
    uart0_tx_buffer((u8_t*)ptr, len);
    return len;
  }
#endif
  return SPIFFS_write(&fs, (spiffs_file)fd, (void *)ptr, len);
}
size_t ICACHE_FLASH_ATTR myspiffs_read( int fd, void* ptr, size_t len){
  return SPIFFS_read(&fs, (spiffs_file)fd, ptr, len);
}
int ICACHE_FLASH_ATTR myspiffs_lseek( int fd, int off, int whence ){
  return SPIFFS_lseek(&fs, (spiffs_file)fd, off, whence);
}
int ICACHE_FLASH_ATTR myspiffs_eof( int fd ){
  return SPIFFS_eof(&fs, (spiffs_file)fd);
}
int ICACHE_FLASH_ATTR myspiffs_tell( int fd ){
  return SPIFFS_tell(&fs, (spiffs_file)fd);
}
int ICACHE_FLASH_ATTR myspiffs_getc( int fd ){
  char c = EOF;
  if(!myspiffs_eof(fd)){
    SPIFFS_read(&fs, (spiffs_file)fd, &c, 1);
  }
  return (int)c;
}
int ICACHE_FLASH_ATTR myspiffs_ungetc( int c, int fd ){
  return SPIFFS_lseek(&fs, (spiffs_file)fd, -1, SEEK_CUR);
}
int ICACHE_FLASH_ATTR myspiffs_flush( int fd ){
  return SPIFFS_fflush(&fs, (spiffs_file)fd);
}
int ICACHE_FLASH_ATTR myspiffs_error( int fd ){
  return SPIFFS_errno(&fs);
}
void ICACHE_FLASH_ATTR myspiffs_clearerr( int fd ){
  fs.errno = SPIFFS_OK;
}
#if 0
void ICACHE_FLASH_ATTR test_spiffs() {
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
