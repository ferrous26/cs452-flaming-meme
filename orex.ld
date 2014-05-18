ENTRY (main)

MEMORY
{
  ram : ORIGIN = 0x00001000, LENGTH = 31M
  rom : ORIGIN = 0x60000000, LENGTH = 8M
}

SECTIONS
{
  .heap 0x1000 : /* Space to futz with */
  {
    . = . + 25165824; /* allocate 24 MB for dynamic memory use */
  } >ram

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
