/* The faces and the text, in flash. Built with -Wa,-I<dir holding v1.inkf, v2.inkf, book.txt> (run.sh). */
#ifdef __APPLE__ /* the host build of the same program */
#define SYM(x) _##x
    .section __TEXT,__const
#else
#define SYM(x) x
    .section .rodata.font_pack, "a"
#endif
    .balign 64
    .globl SYM(v1_face), SYM(v1_face_end)
SYM(v1_face):
    .incbin "v1.inkf"
SYM(v1_face_end):
    .balign 64
    .globl SYM(v2_face), SYM(v2_face_end)
SYM(v2_face):
    .incbin "v2.inkf"
SYM(v2_face_end):
    .balign 4
    .globl SYM(book), SYM(book_end)
SYM(book):
    .incbin "book.txt"
SYM(book_end):
