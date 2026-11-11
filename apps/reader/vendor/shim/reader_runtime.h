#pragma once
#include <cstdio>
extern "C" void ink_reader_host_call(void (*fn)(void *), void *arg);
/* Only the imported ZIP core uses these FILE-shaped read-only handles. Declared at global scope and brought into std:
 * the macros below also reach unqualified uses in later system headers (glibc names fclose in a dealloc attribute),
 * and GCC then needs a global declaration; the vendor code calls them as std::fopen and so on. */
FILE *ink_reader_fopen(const char *, const char *);
int ink_reader_fclose(FILE *);
size_t ink_reader_fread(void *, size_t, size_t, FILE *);
int ink_reader_fseek(FILE *, long, int);
long ink_reader_ftell(FILE *);
int ink_reader_ferror(FILE *);
namespace std {
using ::ink_reader_fopen;
using ::ink_reader_fclose;
using ::ink_reader_fread;
using ::ink_reader_fseek;
using ::ink_reader_ftell;
using ::ink_reader_ferror;
}
#define fopen ink_reader_fopen
#define fclose ink_reader_fclose
#define fread ink_reader_fread
#define fseek ink_reader_fseek
#define ftell ink_reader_ftell
#define ferror ink_reader_ferror
