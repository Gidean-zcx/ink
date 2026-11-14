/* home_library.c picks random books with esp_random(); the device has one too, so this one has its own name. */
#pragma once
#include <stdint.h>
uint32_t ink_mogo_lock_random(void);
#define esp_random ink_mogo_lock_random
