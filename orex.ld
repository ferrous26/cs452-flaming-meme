ENTRY (main)

MEMORY
{
  ram : ORIGIN = 0x00100000, LENGTH = 31M
  rom : ORIGIN = 0x60000000, LENGTH = 8M
}

SECTIONS
{
  .data : /* Initialized data. */
  {
    _DataStart = . ;
    *(.data.kern)
    _DataKernEnd = . ;
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

  .text ALIGN(0x100): 	/* The actual instructions.          */
  {		      	/* Alignment ensures that data will  */
    _TextStart = . ;	/* not get loaded into the icache    */
    *(.text.kern)
    _TextKernEnd = . ;
    *(.text)
    *(.rodata)
    *(.rodata.str1.4)
    *(.glue_7)
    *(.glue_7t)
    _TextEnd = . ;
  } >ram
}
