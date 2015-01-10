#ifndef __FLASH_API_H__
#define __FLASH_API_H__
#include "ets_sys.h"
#include "user_config.h"
#include "cpu_esp8266.h"
#define FLASH_MAP_START_ADDRESS (INTERNAL_FLASH_START_ADDRESS)

/******************************************************************************
 * ROM Function definition
 * Note: It is unsafe to use ROM function, but it may efficient.
 * SPIEraseSector
 * unknown SPIEraseSector(uint16 sec);
 * The 1st parameter is flash sector number.

 * SPIRead (Unsafe)
 * unknown SPIRead(uint32_t src_addr, uint32_t *des_addr, uint32_t size);
 * The 1st parameter is source addresses.
 * The 2nd parameter is destination addresses.
 * The 3rd parameter is size.
 * Note: Sometimes it have no effect, may be need a delay or other option(lock or unlock, etc.) with known reason.

 * SPIWrite (Unsafe)
 * unknown SPIWrite(uint32_t des_addr, uint32_t *src_addr, uint32_t size);
 * The 1st parameter is destination addresses.
 * The 2nd parameter is source addresses.
 * The 3rd parameter is size.
 * Note: Sometimes it have no effect, may be need a delay or other option(lock or unlock, etc.) with known reason.
*******************************************************************************/

typedef struct
{
    uint8_t unknown0;
    uint8_t unknown1;
    enum
    {
        MODE_QIO = 0,
        MODE_QOUT = 1,
        MODE_DIO = 2,
        MODE_DOUT = 15,
    } mode : 8;
    enum
    {
        SPEED_40MHZ = 0,
        SPEED_26MHZ = 1,
        SPEED_20MHZ = 2,
        SPEED_80MHZ = 15,
    } speed : 4;
    enum
    {
        SIZE_4MBIT = 0,
        SIZE_2MBIT = 1,
        SIZE_8MBIT = 2,
        SIZE_16MBIT = 3,
        SIZE_32MBIT = 4,
    } size : 4;
} ICACHE_STORE_TYPEDEF_ATTR SPIFlashInfo;

SPIFlashInfo flash_get_info(void);
uint8_t flash_get_size(void);
uint32_t flash_get_size_byte(void);
bool flash_set_size(uint8_t);
bool flash_set_size_byte(uint32_t);
uint16_t flash_get_sec_num(void);
uint8_t flash_get_mode(void);
uint32_t flash_get_speed(void);
bool flash_init_data_written(void);
bool flash_init_data_default(void);
bool flash_init_data_blank(void);
bool flash_self_destruct(void);
uint8_t byte_of_aligned_array(const uint8_t* aligned_array, uint32_t index);

#endif // __FLASH_API_H__
