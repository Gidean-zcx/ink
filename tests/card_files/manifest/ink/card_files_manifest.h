/* A test list for tests/card_files/unit.c, written by hand: the generator refuses its bad paths, the firmware must too. */
#ifndef INK_CARD_FILES_MANIFEST_H
#define INK_CARD_FILES_MANIFEST_H
#include <stdint.h>
typedef struct { const char *path; uint32_t size; uint8_t sha256[32]; const char *app; } ink_card_file_t;
static const ink_card_file_t INK_CARD_FILES[] = {
    {"/content/fonts/Test.ttf", 5000u, {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
                                        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef}, "reader"},
    {"/user/sys/x", 10u, {0}, "reader"},
    {"/content/../x", 10u, {0}, "reader"},
    {"/content/.m/owner/x", 10u, {0}, "reader"},
    {"/content/fonts/x.part", 10u, {0}, "reader"},
    {"/content/.system/a//b", 10u, {0}, "other"},
};
#define INK_CARD_FILES_COUNT 6
#define INK_CARD_FILES_DIGEST "00112233aabbccdd"
#endif
