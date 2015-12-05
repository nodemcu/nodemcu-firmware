/* This linker script generated from xt-genldscripts.tpp for LSP . */
/* Linker Script for ld -N */
MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  iram1_0_seg :                         org = 0x40100000, len = 0x8000
  irom0_0_seg :                         org = 0x40210000, len = 0x80000
}

PHDRS
{
  dport0_0_phdr PT_LOAD;
  dram0_0_phdr PT_LOAD;
  dram0_0_bss_phdr PT_LOAD;
  iram1_0_phdr PT_LOAD;
  irom0_0_phdr PT_LOAD;
}


/*  Default entry point:  */
ENTRY(user_start_trampoline)
EXTERN(_DebugExceptionVector)
EXTERN(_DoubleExceptionVector)
EXTERN(_KernelExceptionVector)
EXTERN(_NMIExceptionVector)
EXTERN(_UserExceptionVector)
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

  .irom0.text : ALIGN(4)
  {
    _irom0_text_start = ABSOLUTE(.);
    *(.irom0.literal .irom.literal .irom.text.literal .irom0.text .irom.text)
    *(.literal.* .text.*)
    *(.rodata*)
    *(.sdk.version)

    /* These are *only* pulled in by Lua, and therefore safe to put in flash */
    */libc.a:lib_a-isalnum.o(.text* .literal*)
    */libc.a:lib_a-isalpha.o(.text* .literal*)
    */libc.a:lib_a-iscntrl.o(.text* .literal*)
    */libc.a:lib_a-isspace.o(.text* .literal*)
    */libc.a:lib_a-islower.o(.text* .literal*)
    */libc.a:lib_a-isupper.o(.text* .literal*)
    */libc.a:lib_a-ispunct.o(.text* .literal*)
    */libc.a:lib_a-isxdigit.o(.text* .literal*)
    */libc.a:lib_a-locale.o(.text* .literal*)
    */libc.a:lib_a-tolower.o(.text* .literal*)
    */libc.a:lib_a-toupper.o(.text* .literal*)
    */libc.a:lib_a-strcasecmp.o(.text* .literal*)
    */libc.a:lib_a-strcoll.o(.text* .literal*)
    */libc.a:lib_a-strchr.o(.text* .literal*)
    */libc.a:lib_a-strrchr.o(.text* .literal*)
    */libc.a:lib_a-strcat.o(.text* .literal*)
    */libc.a:lib_a-strncat.o(.text* .literal*)
    */libc.a:lib_a-strcspn.o(.text* .literal*)
    */libc.a:lib_a-strtol.o(.text* .literal*)
    */libc.a:lib_a-strtoul.o(.text* .literal*)
    */libc.a:lib_a-strpbrk.o(.text* .literal*)
    */libc.a:lib_a-memchr.o(.text* .literal*)
    */libc.a:lib_a-setjmp.o(.text* .literal*)
    /* end Lua C lib functions */

    _irom0_text_end = ABSOLUTE(.);
    _flash_used_end = ABSOLUTE(.);
  } >irom0_0_seg :irom0_0_phdr

  .data : ALIGN(4)
  {
    _data_start = ABSOLUTE(.);
    *(.data)

    /* Hook for randomizing TCP start numbers */
    . = ALIGN(2);
    _tcp_new_port_port = ABSOLUTE(.);
    */liblwip.a:tcp.o(.port)
    /* Verify that the LWIP source has been patched as needed, or fail with
     * a divide by zero error (../ld/nodemcu.ld:xxx / by zero) */
    _tcp_new_port_port_size = ABSOLUTE(.) - _tcp_new_port_port;
    _ensure_tcp_port_randomization_fix_presence = 1 / _tcp_new_port_port_size;

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
    /*  C++ constructor and destructor tables, properly ordered:  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
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
    . = ALIGN(4);		/* this table MUST be 4-byte aligned */
    _bss_table_start = ABSOLUTE(.);
    LONG(_bss_start)
    LONG(_bss_end)
    _bss_table_end = ABSOLUTE(.);
    _rodata_end = ABSOLUTE(.);
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
    _heap_start = ABSOLUTE(.);
/*    _stack_sentry = ALIGN(0x8); */
  } >dram0_0_seg :dram0_0_bss_phdr
/* __stack = 0x3ffc8000; */

  .text : ALIGN(4)
  {
    _stext = .;
    _text_start = ABSOLUTE(.);
    *(.UserEnter.text)
    . = ALIGN(16);
    *(.DebugExceptionVector.text)
    . = ALIGN(16);
    *(.NMIExceptionVector.text)
    . = ALIGN(16);
    *(.KernelExceptionVector.text)
    LONG(0)
    LONG(0)
    LONG(0)
    LONG(0)
    . = ALIGN(16);
    *(.UserExceptionVector.text)
    LONG(0)
    LONG(0)
    LONG(0)
    LONG(0)
    . = ALIGN(16);
    *(.DoubleExceptionVector.text)
    LONG(0)
    LONG(0)
    LONG(0)
    LONG(0)
    . = ALIGN (16);
    *(.entry.text)
    *(.init.literal)
    *(.init)
    *(.literal .text .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.iram0.text)
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;
  } >iram1_0_seg :iram1_0_phdr

  .lit4 : ALIGN(4)
  {
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
  } >iram1_0_seg :iram1_0_phdr

}

/* get ROM code address */
INCLUDE "eagle.rom.addr.v6.ld"
