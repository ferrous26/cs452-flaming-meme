ENTRY (main)

MEMORY
{
  ram : ORIGIN = 0x00000000, LENGTH = 32M
  rom : ORIGIN = 0x60000000, LENGTH = 8M
}

SECTIONS
{
  .data 0x100000: /* Initialized data. */
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

  .rodata :
  {
    _RODataStart = . ;
    *(.rodata)
    *(.rodata.str1.4)
    _RODataEnd = . ;
  } >ram

  .text ALIGN(0x100): 	/* The actual instructions.          */
  {		      	/* Alignment ensures that data will  */
    _TextStart = . ;	/* not get loaded into the icache    */
    *(.text.kern)
    _TextKernEnd = . ;
    *(.text)
    *(.glue_7)		/* Glue sections hold code for thumb interworking */
    *(.glue_7t)
    _TextColdStart = . ;
    *(.text.cold)
    _TextEnd = . ;
  } >ram
}
