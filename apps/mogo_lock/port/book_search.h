#pragma once
#include <stdbool.h>
#include <string.h>
static inline bool book_search_match(const char *name, const char *query) { return !query || !*query || strstr(name, query); }
