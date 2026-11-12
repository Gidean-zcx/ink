/* Preserve native source-byte positions while adding the requested UTF-16 input. */
#include "book_source.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ttf_font_open_mem(uint8_t *data, size_t len, const char *label) {
    (void)len; (void)label;
    free(data);
    return -1;
}
void ttf_font_close_embedded(void) {}
int main(int argc, char **argv) {
    if (argc != 4) return 2;
    FILE *expected = fopen(argv[2],"rb");
    if (!expected) return 3;
    if (book_open(argv[1]) != ESP_OK || book_chapter_count() != 2 ||
        book_chapter_byte_offset(1) != (uint32_t)strtoul(argv[3],NULL,10)) return 4;
    char title[128];
    if (book_chapter_title(1,title,sizeof title) != ESP_OK || strcmp(title,"第二章 灯下")) return 5;
    for (size_t chapter = 0; chapter < book_chapter_count(); chapter++) {
        char *text = NULL;
        size_t length = 0;
        if (book_chapter_load(chapter,&text,&length) != ESP_OK || !strstr(text,"星光")) return 6;
        for (size_t at = 0; at < length; at++) {
            if (fgetc(expected) != (unsigned char)text[at]) return 7;
        }
        free(text);
    }
    if (fgetc(expected) != EOF || ferror(expected) || fclose(expected)) return 8;
    book_close();
    puts("UTF-16 decoding, chapter navigation and original byte offsets passed");
    return 0;
}
