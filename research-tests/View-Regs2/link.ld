OUTPUT_ARCH( "riscv" )
ENTRY( _start )

SECTIONS
{
  /* 1. Start at the beginning of Spike's RAM */
  . = 0x80000000;
  
  /* 2. Place Code and Data */
  .text : { *(.text.init) *(.text) }
  .data : { *(.data) }
  .bss  : { *(.bss) }
  
  /* 3. Align end of data */
  . = ALIGN(16);
  _end = .;
  
  /* 4. Define Stack Top 64KB higher to ensure no overlap */
  . = . + 0x10000;
  stack_top = .;
}