OUTPUT_ARCH( "riscv" )
ENTRY( _start )

SECTIONS
{
  . = 0x80000000;
  .text : { *(.text.init) *(.text) *(.text.*) }
  .rodata : { *(.rodata) *(.rodata.*) }
  .data : { *(.data) *(.data.*) }
  .bss  : { *(.bss) *(.bss.*) *(COMMON) }
  . = ALIGN(16);
  _end = .;
  _heap_end = _end + 0x10000;
  . = . + 0x10000;
  stack_top = .;
  /* Port expects this for the ISR stack when configISR_STACK_SIZE_WORDS is not set. */
  __freertos_irq_stack_top = .;

  /* Spike requires tohost/fromhost in ELF for host-target communication. */
  . = ALIGN(8);
  .tohost : {
    tohost = .;
    . += 8;
    fromhost = .;
    . += 8;
  }
}
