#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "cpu_esp32.h"

#define PLATFORM_ALIGNMENT __attribute__((aligned(4)))
#define PLATFORM_ALIGNMENT_PACKED __attribute__((aligned(4),packed))

// Error / status codes
enum
{
  PLATFORM_ERR,
  PLATFORM_OK,
  PLATFORM_UNDERFLOW = -1
};


#if CONFIG_NODE_DEBUG
# define NODE_DBG printf
#else
# define NODE_DBG(...) do{}while(0)
#endif

#if CONFIG_NODE_ERR
# define NODE_ERR printf
#else
# define NODE_ERR(...) do{}while(0)
#endif


int platform_init (void);

// *****************************************************************************
// UART subsection

// Parity
enum
{
  PLATFORM_UART_PARITY_NONE  = 0,
  PLATFORM_UART_PARITY_EVEN  = 1,
  PLATFORM_UART_PARITY_ODD   = 2,
  PLATFORM_UART_PARITY_MARK  = 3,
  PLATFORM_UART_PARITY_SPACE = 4
};


// Stop bits
enum
{
  PLATFORM_UART_STOPBITS_1   = 1,
  PLATFORM_UART_STOPBITS_2   = 2,
  PLATFORM_UART_STOPBITS_1_5 = 3
};

// Flow control types (this is a bit mask, one can specify PLATFORM_UART_FLOW_RTS | PLATFORM_UART_FLOW_CTS )
#define PLATFORM_UART_FLOW_NONE               0
#define PLATFORM_UART_FLOW_RTS                1
#define PLATFORM_UART_FLOW_CTS                2

// The platform UART functions
static inline int platform_uart_exists( unsigned id ) { return id < NUM_UART; }
uint32_t platform_uart_setup( unsigned id, uint32_t baud, int databits, int parity, int stopbits );
void platform_uart_send( unsigned id, uint8_t data );
int platform_uart_set_flow_control( unsigned id, int type );


// Internal flash erase/write functions

uint32_t platform_flash_get_first_free_block_address( uint32_t *psect );
uint32_t platform_flash_get_sector_of_address( uint32_t addr );
uint32_t platform_flash_write( const void *from, uint32_t toaddr, uint32_t size );
uint32_t platform_flash_read( void *to, uint32_t fromaddr, uint32_t size );
uint32_t platform_s_flash_write( const void *from, uint32_t toaddr, uint32_t size );
uint32_t platform_s_flash_read( void *to, uint32_t fromaddr, uint32_t size );
uint32_t platform_flash_get_num_sectors(void);
int platform_flash_erase_sector( uint32_t sector_id );

/**
 * Translated a mapped address to a physical flash address, based on the
 * current flash cache mapping.
 * @param mapped_addr Address to translate (>= INTERNAL_FLASH_MAPPED_ADDRESS)
 * @return the corresponding physical flash address, or -1 if flash cache is
 *  not currently active.
 * @see Cache_Read_Enable.
 */
uint32_t platform_flash_mapped2phys (uint32_t mapped_addr);


// Internal flash partitions
typedef struct {
  uint8_t  label[16];
  uint32_t offs;
  uint32_t size;
  uint8_t  type;
  uint8_t  subtype;
} platform_partition_t;

/**
 * Obtain partition information for the internal flash.
 * @param idx Which partition index to load info for.
 * @param info Buffer to store the info in.
 * @returns True if the partition info was loaded, false if not (e.g. no such
 *   partition idx).
 */
bool platform_partition_info (uint8_t idx, platform_partition_t *info);



// *****************************************************************************
// Helper macros
#define MOD_CHECK_ID( mod, id )\
  if( !platform_ ## mod ## _exists( id ) )\
    return luaL_error( L, #mod" %d does not exist", ( unsigned )id )

#define MOD_CHECK_TIMER( id )\
  if( id == PLATFORM_TIMER_SYS_ID && !platform_timer_sys_available() )\
    return luaL_error( L, "the system timer is not available on this platform" );\
  if( !platform_tmr_exists( id ) )\
    return luaL_error( L, "timer %d does not exist", ( unsigned )id )\

#define MOD_CHECK_RES_ID( mod, id, resmod, resid )\
  if( !platform_ ## mod ## _check_ ## resmod ## _id( id, resid ) )\
    return luaL_error( L, #resmod" %d not valid with " #mod " %d", ( unsigned )resid, ( unsigned )id )



#endif
