/**
 * define start/end address of ro data.
 * different compiler with different implementation.
 */

#ifndef __COMPILER_H__
#define __COMPILER_H__

#if defined(__CC_ARM)       // armcc

//#warning "Please check scatter file to ensure rodata is in ER_IROM1 region."

/* symbols reference to the scatter file */
extern char Image$$ER_IROM1$$Base;
extern char Image$$ER_IROM1$$Limit;

#define RODATA_START_ADDRESS        (&Image$$ER_IROM1$$Base)
#define RODATA_END_ADDRESS          (&Image$$ER_IROM1$$Limit)

#elif defined(__GNUC__)     // gcc

//#warning "Please check linker script to ensure rodata is between _stext and _etext."

/* symbols defined in linker script */
// extern char _rodata_start;
// extern char _rodata_end;
extern char _irom0_text_start;
extern char _irom0_text_end;
// modify linker script to ensure rodata and rodata1 is between _rodata_start and _rodata_end.
// #define RODATA_START_ADDRESS        (&_rodata_start)
// #define RODATA_END_ADDRESS          (&_rodata_end)
#define RODATA_START_ADDRESS        (&_irom0_text_start)
#define RODATA_END_ADDRESS          (&_irom0_text_end)

#else                       // other compilers

/* Firstly, modify rodata's start/end address. Then, comment the line below */
#error "Please modify RODATA_START_ADDRESS and RODATA_END_ADDRESS below."

/* Perhaps you can use start/end address of flash */
#define RODATA_START_ADDRESS        ((char*)0x40200000)
#define RODATA_END_ADDRESS          ((char*)0x40280000)

#endif

#endif // __COMPILER_H__

