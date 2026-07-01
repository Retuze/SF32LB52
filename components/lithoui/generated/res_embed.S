    .section .rodata,"a",%progbits
    .globl _binary_res_images_bin_start
    .globl _binary_res_images_bin_end
    .balign 4
_binary_res_images_bin_start:
    /* Relative path — resolved via the assembler -I include dir
     * (components/lithoui/generated, added in CMakeLists.txt).
     * Must NOT be an absolute machine-specific path. */
    .incbin "res_images.bin"
_binary_res_images_bin_end:
