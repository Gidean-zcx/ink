#pragma once
#include <cstdio>
extern "C" void ink_reader_host_call(void (*fn)(void *), void *arg);
/* Only the imported ZIP core uses these FILE-shaped read-only handles. */
namespace std {
FILE *ink_reader_fopen(const char *, const char *);
int ink_reader_fclose(FILE *);
size_t ink_reader_fread(void *, size_t, size_t, FILE *);
int ink_reader_fseek(FILE *, long, int);
long ink_reader_ftell(FILE *);
int ink_reader_ferror(FILE *);
}
#define fopen ink_reader_fopen
#define fclose ink_reader_fclose
#define fread ink_reader_fread
#define fseek ink_reader_fseek
#define ftell ink_reader_ftell
#define ferror ink_reader_ferror
