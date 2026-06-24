/* Bootloader linker script — loaded by ROM from flash, executed in RAM
 *
 * ROM bootloader copies the binary from flash (LMA) to RAM (VMA), then
 * jumps to the reset handler at the VMA.
 *
 * Flash: 0x12010000  64 KB  (storage)
 * RAM:   0x20020000  64 KB  (execution)
 */

__ROM_BASE    = 0x12010000;
__ROM_SIZE    = 0x00010000;  /* 64 KB */
__RAM_BASE    = 0x20020000;
__RAM_SIZE    = 0x00010000;  /* 64 KB */
__STACK_SIZE  = 0x00001000;
__HEAP_SIZE   = 0x00000000;

APPLICATION_ADDR = 0x12020000;

__StackTop    = __RAM_BASE + __RAM_SIZE;
__StackLimit  = __StackTop - __STACK_SIZE;

MEMORY
{
    FLASH (rx)  : ORIGIN = __ROM_BASE, LENGTH = __ROM_SIZE
    RAM   (rwx) : ORIGIN = __RAM_BASE, LENGTH = __RAM_SIZE
}

ENTRY(Reset_Handler)

SECTIONS
{
    .vectors :
    {
        KEEP(*(.vectors))
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > RAM AT> FLASH

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
    } > RAM AT> FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > RAM AT> FLASH

    .data :
    {
        __data_start__ = .;
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH
    __data_load__ = LOADADDR(.data);

    /* ----- Picolibc TLS ----- */
    .tdata : ALIGN(4)
    {
        __tdata_start = .;
        *(.tdata .tdata.* .gnu.linkonce.td.*)
        __tdata_end = .;
        __data_end__ = .;
    } > RAM AT> FLASH

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
        . = . + __HEAP_SIZE;
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
    } > RAM AT> FLASH

    .zero.table :
    {
        . = ALIGN(4);
        __zero_table_start__ = .;
        LONG(__tbss_start)
        LONG(__tbss_end - __tbss_start)
        LONG(_sbss)
        LONG(_ebss - _sbss)
        __zero_table_end__ = .;
    } > RAM AT> FLASH

    __exidx_start = .;
    .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > RAM AT> FLASH
    __exidx_end = .;
}

PROVIDE(__stack = __StackTop);
