#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "graft.h"
bool ink_mogo_page_open(const char *path, const ink_member_identity_t *owner, int w, int h, int px);
void ink_mogo_page_turn(int direction);
void ink_mogo_page_draw(uint8_t *pixels);
void ink_mogo_page_close(void);
int ink_mogo_page_snapshot(char *out, size_t cap);
