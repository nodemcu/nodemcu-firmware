#ifndef _SECTIONS_H_
#define _SECTIONS_H_

#define TEXT_SECTION_ATTR __attribute__((section(".iram0.text")))
#define RAM_CONST_SECTION_ATTR __attribute((section(".rodata.dram")))

#endif
