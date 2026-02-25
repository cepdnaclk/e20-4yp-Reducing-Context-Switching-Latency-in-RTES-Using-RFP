ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128K
}

SECTIONS
{
    .text : {
        *(.text*)
    } > RAM

    .rodata : {
        *(.rodata*)
    } > RAM

    .data : {
        *(.data*)
    } > RAM

    .bss : {
        *(.bss*)
        *(COMMON)
    } > RAM

    /* HTIF host/target communication symbols for Spike console */
    .tohost : {
        . = ALIGN(8);
        KEEP(*(.tohost))
    } > RAM

    .fromhost : {
        . = ALIGN(8);
        KEEP(*(.fromhost))
    } > RAM

    .stack : {
        . = ALIGN(16);
        _stack_top = . + 4K;
        __freertos_irq_stack_top = _stack_top;
    } > RAM
}
