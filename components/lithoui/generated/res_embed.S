    .section .rodata,"a",%progbits
    .globl _binary_res_images_bin_start
    .globl _binary_res_images_bin_end
    .balign 4
_binary_res_images_bin_start:
    .incbin "D:/SIFLI/SF32LB52/components/lithoui/generated/res_images.bin"
_binary_res_images_bin_end:
