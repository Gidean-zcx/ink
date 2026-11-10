#include "display_idf.h"
#define LCD_LL_GET_HW(id) ((void *)(uintptr_t)(id))
void lcd_ll_stop(void *hw);
