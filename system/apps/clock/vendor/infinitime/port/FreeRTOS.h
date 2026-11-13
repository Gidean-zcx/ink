/* clock-3 shim for InfiniTime's <FreeRTOS.h> (SOURCE.md): a tick is a millisecond of the monotonic clock that
 * clock_infinitime.cpp names before including this. Only <stdint.h>; nothing here has external linkage. The guard is
 * FreeRTOS's own, so the SDK's header can never be read after this one. */
#ifndef INC_FREERTOS_H
#define INC_FREERTOS_H
#define INK_INFINITIME_SHIM 1
#include <stdint.h>
typedef uint32_t TickType_t; /* 1000 hours (3.6e9 ms, the stopwatch's wrap) fit */
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 1000
#endif
#ifndef pdTRUE
#define pdTRUE ((BaseType_t)1)
#endif
#ifndef pdFALSE
#define pdFALSE ((BaseType_t)0)
#endif
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#endif
#ifndef INK_INFINITIME_NOW_US
#error "clock_infinitime.cpp names the clock (INK_INFINITIME_NOW_US) before including the shim"
#endif
static inline TickType_t xTaskGetTickCount(void) { return (TickType_t)(INK_INFINITIME_NOW_US() / 1000); }
#endif
