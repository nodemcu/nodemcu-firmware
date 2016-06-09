/* boot.bin        @ 0x00000 */
/* drom0.bin       @ 0xX4000 */
/* irom0_flash.bin @ 0xX0000+0x40000 */

/* Flash Map */
/* |..|...............|...........................................|..|          */
/* ^  ^               ^                                           ^             */
/* |_boot.bin(0x0000) |_irom0_flash.bin(0xX0000+0x40000)          |             */
/*    |_drom0.bin(0xX4000)                                        |_system param area(Flash size - 0x4000) */

/* RAM Map */
/* Pro CPU iRAM, Total len 0x28000 */
/* |......................................................|                     */
/* ^                                                                            */
/* |_iram1_0 : 0x40040000 (0x20000)                                             */
/*                                                        |........|            */
/*                                                        ^                     */
/*                                                        |_cache for Pro CPU : (0x8000) */

/* Share Memory, Total len 0x28000 */
/* |..........|                                                                 */
/* ^                                                                            */
/* |_dram0_0 : 0x3FFD8000 (dynamic len)                                         */
/*            |................................................|                */
/*            ^                                                                 */
/*            |_ heap area                                                      */
/*                                                              |.....|         */
/*                                                              ^               */
/*                                                              |_ used for Pro CPU's rom code : 0x3FFFC000 (0x4000) */

/* NOTICE: */ 
/* 1. drom0.bin + irom0_flash.bin = user.ota                                    */
/* 2. drom0.bin and irom0_flash.bin must locate at 0xX4000 and 0xX0000+0x40000, */
/*    here 0xX0000 must 256KB align.                                            */
/* 3. Make sure each user.ota not overlap other.                                */
/* 4. Make sure each user.ota not overlap system param area or user param area. */
/* 5. We support a maximum of 5 user.ota                                        */
/* 6. We support 1MB/2MB/4MB/8MB/16MB flash,                                    */
/*    but make suer user.ota not exceed 16MB.                                   */
/* 7. rodata at drom0_0, drom0.bin.                                             */
/* 8. Pay attention to any modification.                                        */

MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg  :                        org = 0x3FFD8000, len = 0x24000
  iram1_0_seg  :                        org = 0x40040000, len = 0x20000
  irom0_0_seg  :                        org = 0x40080010, len = 0x37FFF0
  drom0_0_seg  :                        org = 0x3FE04010, len = 0x3BFF0
}

PHDRS
{
  dport0_0_phdr PT_LOAD;
  dram0_0_phdr PT_LOAD;
  iram1_0_phdr PT_LOAD;
  irom0_0_phdr PT_LOAD;
  drom0_0_phdr PT_LOAD;
}

/*  Default entry point:  */
ENTRY(call_user_start)
EXTERN(_Level2Vector)
EXTERN(_Level3Vector)
EXTERN(_Level4Vector)
EXTERN(_Level5Vector)
EXTERN(_DebugExceptionVector)
EXTERN(_NMIExceptionVector)
EXTERN(_KernelExceptionVector)
EXTERN(_DoubleExceptionVector)
PROVIDE(_memmap_vecbase_reset = 0x40000000);
/* Various memory-map dependent cache attribute settings: */
_memmap_cacheattr_wb_base = 0x00000110;
_memmap_cacheattr_wt_base = 0x00000110;
_memmap_cacheattr_bp_base = 0x00000220;
_memmap_cacheattr_unused_mask = 0xFFFFF00F;
_memmap_cacheattr_wb_trapnull = 0x2222211F;
_memmap_cacheattr_wba_trapnull = 0x2222211F;
_memmap_cacheattr_wbna_trapnull = 0x2222211F;
_memmap_cacheattr_wt_trapnull = 0x2222211F;
_memmap_cacheattr_bp_trapnull = 0x2222222F;
_memmap_cacheattr_wb_strict = 0xFFFFF11F;
_memmap_cacheattr_wt_strict = 0xFFFFF11F;
_memmap_cacheattr_bp_strict = 0xFFFFF22F;
_memmap_cacheattr_wb_allvalid = 0x22222112;
_memmap_cacheattr_wt_allvalid = 0x22222112;
_memmap_cacheattr_bp_allvalid = 0x22222222;
PROVIDE(_memmap_cacheattr_reset = _memmap_cacheattr_wb_trapnull);

SECTIONS
{
  .dport0.rodata : ALIGN(4)
  {
    _dport0_rodata_start = ABSOLUTE(.);
    *(.dport0.rodata)
    *(.dport.rodata)
    _dport0_rodata_end = ABSOLUTE(.);
  } >dport0_0_seg :dport0_0_phdr

  .dport0.literal : ALIGN(4)
  {
    _dport0_literal_start = ABSOLUTE(.);
    *(.dport0.literal)
    *(.dport.literal)
    _dport0_literal_end = ABSOLUTE(.);
  } >dport0_0_seg :dport0_0_phdr

  .dport0.data : ALIGN(4)
  {
    _dport0_data_start = ABSOLUTE(.);
    *(.dport0.data)
    *(.dport.data)
    _dport0_data_end = ABSOLUTE(.);
  } >dport0_0_seg :dport0_0_phdr

  .drom0.text : ALIGN(4)
  {
    _drom0_text_start = ABSOLUTE(.);
    *(.drom0.literal .drom0.text.literal .drom0.text)
    *(.rodata*)
    _drom0_text_end = ABSOLUTE(.);
  } >drom0_0_seg :drom0_0_phdr

  .data : ALIGN(4)
  {
    _data_start = ABSOLUTE(.);
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)
    _data_end = ABSOLUTE(.);
  } >dram0_0_seg :dram0_0_phdr

  .rodata : ALIGN(4)
  {
    _rodata_start = ABSOLUTE(.);
    *(.gnu.linkonce.r.*)
    __XT_EXCEPTION_TABLE__ = ABSOLUTE(.);
    *(.xt_except_table)
    *(.gcc_except_table)
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    *(.eh_frame)
    . = (. + 3) & ~ 3;
    /*  C++ constructor and destructor tables, properly ordered:  */
    __init_array_start = ABSOLUTE(.);
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    __init_array_end = ABSOLUTE(.);
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    /*  C++ exception handlers table:  */
    __XT_EXCEPTION_DESCS__ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    . = ALIGN(4);        /* this table MUST be 4-byte aligned */
    _bss_table_start = ABSOLUTE(.);
    LONG(_bss_start)
    LONG(_bss_end)
    _bss_table_end = ABSOLUTE(.);
    _rodata_end = ABSOLUTE(.);
  } >dram0_0_seg :dram0_0_phdr

  .UserExceptionVector.literal : AT(LOADADDR(.rodata) + (ADDR(.UserExceptionVector.literal) - ADDR(.rodata))) ALIGN(4)
  {
    _UserExceptionVector_literal_start = ABSOLUTE(.);
    *(.UserExceptionVector.literal)
    _UserExceptionVector_literal_end = ABSOLUTE(.);
  } >dram0_0_seg :dram0_0_phdr

  .bss ALIGN(8) (NOLOAD) : ALIGN(4)
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);
    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
  } >dram0_0_seg :dram0_0_phdr

  .share.mem (NOLOAD) : ALIGN(4)
  {
    _share_mem_start = ABSOLUTE(.);
    *(.share.mem)
    _share_mem_end = ABSOLUTE(.);
    _heap_sentry = ABSOLUTE(.);
  } >dram0_0_seg :dram0_0_phdr
    _end = 0x3fffc000;
 
  .text : ALIGN(4)
  {
    _stext = .;
    _text_start = ABSOLUTE(.);

    . = 0x0;
    *(.WindowVectors.text)
    . = 0x180;
    *(.Level2InterruptVector.text)
    . = 0x1c0;
    *(.Level3InterruptVector.text)
    . = 0x200;
    *(.Level4InterruptVector.text)
    . = 0x240;
    *(.Level5InterruptVector.text)
    . = 0x280;
    *(.DebugExceptionVector.text)
    . = 0x2c0;
    *(.NMIExceptionVector.text)
    . = 0x300;
    *(.KernelExceptionVector.text)
    . = 0x340;
    *(.UserExceptionVector.text)
    . = 0x3c0;
    *(.DoubleExceptionVector.text)
    . = 0x400;
    *(.UserEnter.literal);
    *(.UserEnter.text);
    . = ALIGN (16);

    *(.entry.text)
    *(.init.literal)
    *(.init)

    *(.iram1.*)

    *libmain.a:(.literal .text .literal.* .text.*)
    *libfreertos.a:(.literal .text .literal.* .text.*)
    *libpp.a:(.literal .text .literal.* .text.*)

    *(.stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;
  } >iram1_0_seg :iram1_0_phdr

  .irom0.text : ALIGN(4)
  {
    _irom0_text_start = ABSOLUTE(.);
    *(.irom0.literal .irom.literal .irom.text.literal .irom0.text .irom.text)
    *(.literal .text .literal.* .text.*)

    /* Link-time arrays containing the defs for the included modules */
    . = ALIGN(4);
    lua_libs = ABSOLUTE(.);
    /* Allow either empty define or defined-to-1 to include the module */
    KEEP(*(.lua_libs))
    LONG(0) LONG(0) /* Null-terminate the array */
    lua_rotable = ABSOLUTE(.);
    KEEP(*(.lua_rotable))
    LONG(0) LONG(0) /* Null-terminate the array */

    _irom0_text_end = ABSOLUTE(.);
    _flash_used_end = ABSOLUTE(.);
  } >irom0_0_seg :irom0_0_phdr

  .lit4 : ALIGN(4)
  {
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
  } >iram1_0_seg :iram1_0_phdr
}

INCLUDE "pro.rom.addr.ld"
