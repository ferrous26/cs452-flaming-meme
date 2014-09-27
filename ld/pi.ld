/* pi.ld - linker script for raspberry pi */
ENTRY (main)

MEMORY
{
  ram : ORIGIN = 0x00000000, LENGTH = 512M
}

SECTIONS
{
    . = 0x8000;    /* Starts at LOADER_ADDR. */
    _start = .;

    .text :
    {
      _text_start = .;
      _text_hot_start = . ;
      *(.text.hot)
      _text_hot_end = . ;
      *(.text)
      _text_cold_start = . ;
      KEEP(*(.text.boot))
      *(.text.cold)
      _text_cold_end = . ;
      _text_end = . ;
    } >ram

    . = ALIGN(4096); /* align to page size */
    .data : /* Initialized data. */
    {
      _data_start = . ;
      _data_hot_start = . ;
      *(.data.hot)
      _data_hot_end = . ;
      _data_warm_start = . ;
      *(.data.warm)
      _data_warm_end = . ;
      *(.data)
      *(.data.rel.local)
      _data_end = . ;
    } >ram

    . = ALIGN(4096); /* align to page size */
    .bss : /* Uninitialized data. */
    {
      _bss_start = . ;
      *(.bss)
      *(.sbss)
      _bss_end = . ;
    } >ram

    . = ALIGN(4096); /* align to page size */
    .rodata :
    {
      _ro_data_start = . ;
      *(.rodata)
      *(.rodata.str1.4)
      _ro_data_end = . ;
    } >ram

    _end = .;
}
