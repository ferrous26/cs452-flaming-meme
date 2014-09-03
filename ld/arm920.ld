/* arm920.ld - linker script for Cirrus EP9301 */
ENTRY (main)

MEMORY
{
  ram : ORIGIN = 0x00000000, LENGTH = 32M
  rom : ORIGIN = 0x60000000, LENGTH = 8M
}

SECTIONS
{
    . = 0x100000; /* Start at this address to avoid overwriting */
    _start = . ;  /* important low memory things */

    .text :
    {
      _text_start = . ;
      _text_hot_start = . ;
      *(.text.hot)
      _text_hot_end = . ;
      *(.text)
      *(.glue_7)	/* Glue sections hold code for thumb interworking */
      *(.glue_7t)
      _text_cold_start = . ;
      *(.text.cold)
      _text_cold_end = . ;
      _text_end  = . ;
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

    _end = . ;
}
