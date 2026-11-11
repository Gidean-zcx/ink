#pragma once
#include "ink/osal.h"
#include "ink/hal_device.h"
inline uint32_t millis() { osal_cost_cycles(320000); return static_cast<uint32_t>(osal_now_us() / 1000); }
inline void yield() { osal_preempt_point(); }
inline long random(long low, long high) {
  uint32_t value; hal_random_fill(&value, sizeof value);
  return low + static_cast<long>(value % static_cast<uint32_t>(high - low));
}
inline long random(long high) { return random(0, high); }
