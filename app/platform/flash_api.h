#ifndef __FLASH_API_H__
#define __FLASH_API_H__
#include "ets_sys.h"
#include "user_config.h"
#include "cpu_esp8266.h"

#define FLASH_SIZE_2MBIT    (2   * 1024 * 1024)
#define FLASH_SIZE_4MBIT    (4   * 1024 * 1024)
#define FLASH_SIZE_8MBIT    (8   * 1024 * 1024)
#define FLASH_SIZE_16MBIT   (16  * 1024 * 1024)
#define FLASH_SIZE_32MBIT   (32  * 1024 * 1024)
#define FLASH_SIZE_64MBIT   (64  * 1024 * 1024)
#define FLASH_SIZE_128MBIT  (128 * 1024 * 1024)

#define FLASH_SIZE_256KBYTE (FLASH_SIZE_2MBIT  / 8)
#define FLASH_SIZE_512KBYTE (FLASH_SIZE_4MBIT  / 8)
#define FLASH_SIZE_1MBYTE   (FLASH_SIZE_8MBIT  / 8)
#define FLASH_SIZE_2MBYTE   (FLASH_SIZE_16MBIT / 8)
#define FLASH_SIZE_4MBYTE   (FLASH_SIZE_32MBIT / 8)
#define FLASH_SIZE_8MBYTE   (FLASH_SIZE_64MBIT / 8)
#define FLASH_SIZE_16MBYTE  (FLASH_SIZE_128MBIT/ 8)

/******************************************************************************
 * ROM Function definition
 * Note: It is unsafe to use ROM function, but it may efficient.
 * SPIEraseSector
 * SpiFlashOpResult  SPIEraseSector(uint16 sec);
 * The 1st parameter is flash sector number.
 * Note: Must disable cache read before using it.

 * SPIRead
 * SpiFlashOpResult  SPIRead(uint32_t src_addr, uint32_t *des_addr, uint32_t size);
 * The 1st parameter is source addresses.
 * The 2nd parameter is destination addresses.
 * The 3rd parameter is size.
 * Note: Must disable cache read before using it.

 * SPIWrite
 * SpiFlashOpResult  SPIWrite(uint32_t des_addr, uint32_t *src_addr, uint32_t size);
 * The 1st parameter is destination addresses.
 * The 2nd parameter is source addresses.
 * The 3rd parameter is size.
 * Note: Must disable cache read before using it.
*******************************************************************************/

typedef struct
{
    uint8_t header_magic;
    uint8_t segment_count;
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
        SIZE_16MBIT_8M_8M = 5,
        SIZE_32MBIT_8M_8M = 6,
        SIZE_32MBIT_16M_16M = 7,
        SIZE_64MBIT = 8,
        SIZE_128MBIT = 9,
    } size : 4;
    uint32_t entry_point;
    uint32_t memory_offset;
    uint32_t segment_size;
} ICACHE_STORE_TYPEDEF_ATTR SPIFlashInfo;

SPIFlashInfo flash_rom_getinfo(void);
uint8_t flash_rom_get_size_type(void);
uint32_t flash_rom_get_size_byte(void);
uint32_t flash_detect_size_byte(void);
bool flash_rom_set_size_type(uint8_t);
bool flash_rom_set_size_byte(uint32_t);
uint16_t flash_rom_get_sec_num(void);
uint8_t flash_rom_get_mode(void);
uint32_t flash_rom_get_speed(void);
uint8_t byte_of_aligned_array(const uint8_t* aligned_array, uint32_t index);
uint16_t word_of_aligned_array(const uint16_t *aligned_array, uint32_t index);
// uint8_t flash_rom_get_checksum(void);
// uint8_t flash_rom_calc_checksum(void);

#endif // __FLASH_API_H__
