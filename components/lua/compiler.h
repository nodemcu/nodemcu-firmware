/**
 * define start/end address of ro data.
 */

#ifndef __COMPILER_H__
#define __COMPILER_H__

#if defined(__ESP8266__)

extern char _irom0_text_start;
extern char _irom0_text_end;
#define RODATA_START_ADDRESS        (&_irom0_text_start)
#define RODATA_END_ADDRESS          (&_irom0_text_end)

#elif defined(CONFIG_IDF_TARGET_ESP32C3)

extern char _rodata_start;
extern char _rodata_end;
#define RODATA_START_ADDRESS        (&_rodata_start)
#define RODATA_END_ADDRESS          (&_rodata_end)

#elif defined(__ESP32__) || defined(CONFIG_IDF_TARGET_ESP32)

#define RODATA_START_ADDRESS        ((char*)0x3F400000)
#define RODATA_END_ADDRESS          ((char*)0x3F800000)

#else                       // other compilers

/* Firstly, modify rodata's start/end address. Then, comment the line below */
#error "Please modify RODATA_START_ADDRESS and RODATA_END_ADDRESS below."

/* Perhaps you can use start/end address of flash */
#define RODATA_START_ADDRESS        ((char*)0x40200000)
#define RODATA_END_ADDRESS          ((char*)0x40280000)

#endif

#endif // __COMPILER_H__

