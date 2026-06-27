/* Linker script for SF32LB52 firmware (Cortex-M33)
 *
 * Memory layout:
 *   ROM: 0x12020000  (14 MB — application space after bootloader)
 *   RAM: 0x20000000  (512 KB SRAM)
 *
 * Provides copy-table / zero-table for startup.c and TLS support for picolibc.
 */

__ROM_BASE    = 0x12020000;
__ROM_SIZE    = 0x00E00000;
__RAM_BASE    = 0x20000000;
__RAM_SIZE    = 0x00080000;
__STACK_SIZE  = 0x00004000;
__HEAP_SIZE   = 0x00018000;  /* 96 KB — PFB tiles ~77 KB + margin */

__StackTop    = __RAM_BASE + __RAM_SIZE;
__StackLimit  = __StackTop - __STACK_SIZE;

MEMORY
{
    ROM (rx)  : ORIGIN = __ROM_BASE, LENGTH = __ROM_SIZE
    RAM (rwx) : ORIGIN = __RAM_BASE, LENGTH = __RAM_SIZE
}

ENTRY(Reset_Handler)

SECTIONS
{
    .vectors :
    {
        KEEP(*(.vectors))
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > ROM

    .text :
    {
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE_HIDDEN(__init_array_end = .);
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);

        . = ALIGN(4);
    } > ROM

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > ROM

    .data :
    {
        __data_start__ = .;
        _sdata = .;
        *(.data)
        *(.data.*)

        . = ALIGN(4);
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)

        . = ALIGN(4);
        _edata = .;
    } > RAM AT>ROM
    __data_load__ = LOADADDR(.data);

    /* ----- Picolibc TLS (Thread Local Storage) ----- */
    .tdata : ALIGN(4)
    {
        __tdata_start = .;
        *(.tdata .tdata.* .gnu.linkonce.td.*)
        __tdata_end = .;
        __data_end__ = .;
    } > RAM AT>ROM

    .tbss (NOLOAD) : ALIGN(4)
    {
        __tbss_start = .;
        *(.tbss .tbss.* .gnu.linkonce.tb.*)
        *(.tcommon)
        __tbss_end = .;
    } > RAM

    PROVIDE(__tdata_source = LOADADDR(.tdata));
    PROVIDE(__tdata_size   = __tdata_end - __tdata_start);
    PROVIDE(__tbss_size    = __tbss_end  - __tbss_start);
    PROVIDE(__tls_base     = __tdata_start);
    PROVIDE(__tls_size     = __tbss_end  - __tdata_start);
    PROVIDE(__tls_align    = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)));
    PROVIDE(__arm32_tls_tcb_offset = MAX(8, __tls_align));
    PROVIDE(__tls_start    = SIZEOF(.tdata) ? ADDR(.tdata) : ADDR(.tbss));
    PROVIDE(__tbss_offset  = ADDR(.tbss) - __tls_start);

    .ramfunc :
    {
        __ramfunc_start__ = .;
        *(.ramfunc)
        *(.ramfunc.*)
        . = ALIGN(4);
        __ramfunc_end__ = .;
    } > RAM AT>ROM
    __ramfunc_load__ = LOADADDR(.ramfunc);

    .bss (NOLOAD) :
    {
        __bss_start__ = .;
        _sbss = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = .;
    } > RAM

    ._user_heap_stack (NOLOAD) :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        PROVIDE(__heap_start = .);
        . = . + __HEAP_SIZE;
        PROVIDE(__heap_end = .);
        . = . + __STACK_SIZE;
        . = ALIGN(8);
    } > RAM

    .copy.table :
    {
        . = ALIGN(4);
        __copy_table_start__ = .;
        LONG(LOADADDR(.data))
        LONG(ADDR(.data))
        LONG(SIZEOF(.data) + SIZEOF(.tdata))
        __copy_table_end__ = .;
    } > ROM

    .zero.table :
    {
        . = ALIGN(4);
        __zero_table_start__ = .;
        LONG(__tbss_start)
        LONG(__tbss_end - __tbss_start)
        LONG(_sbss)
        LONG(_ebss - _sbss)
        __zero_table_end__ = .;
    } > ROM

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > ROM
    __exidx_end = .;
}

PROVIDE(__stack = __StackTop);
