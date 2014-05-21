ENTRY (main)

MEMORY
{
  ram : ORIGIN = 0x00001000, LENGTH = 32M
  rom : ORIGIN = 0x60000000, LENGTH = 8M
}

SECTIONS
{
  .data : /* Initialized data. */
  {
    _DataStart = . ;
    *(.data)
    *(.data.rel.local)
    _DataEnd = . ;
  } >ram

  .bss : /* Uninitialized data. */
  {
    _BssStart = . ;
    *(.bss)
    *(.sbss)
    _BssEnd = . ;
  } >ram

  .text : /* The actual instructions. */
  {
    *(.text)
    *(.got)
    *(.got.plt)
    *(.rodata)
    *(.rodata.str1.4)
    *(.glue_7)
    *(.glue_7t)
  } >ram
}
