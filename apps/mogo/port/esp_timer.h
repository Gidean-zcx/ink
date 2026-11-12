#pragma once
#include "ink/sys/app.h"
#define esp_timer_get_time() ((int64_t)ink_now_us())
