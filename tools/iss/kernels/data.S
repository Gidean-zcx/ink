/* Sample inputs, embedded as read-only data (flash). Built with -Wa,-I<data dir>. */
    .section .rodata.k_data, "a"
    .balign 4
    .globl k_text_zh, k_text_zh_end
k_text_zh:
    .incbin "text_zh.txt"
k_text_zh_end:
    .byte 0
    .balign 4
    .globl k_text_zlib, k_text_zlib_end
k_text_zlib:
    .incbin "text_zh.zlib"
k_text_zlib_end:
    .balign 4
    .globl k_json, k_json_end
k_json:
    .incbin "sample.json"
k_json_end:
    .byte 0
