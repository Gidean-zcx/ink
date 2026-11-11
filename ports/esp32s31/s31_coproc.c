/*
 * ESP-IDF v6.1 FreeRTOS port, RISC-V (components/freertos/FreeRTOS-Kernel/portable/riscv/port.c, pxPortGetCoprocArea),
 * copied with only its placement changed. Original: SPDX-FileCopyrightText Amazon.com, Inc. (MIT) and Espressif
 * Systems (Shanghai) CO LTD (Apache-2.0); see the ESP-IDF tree for the full notices.
 *
 * Why: the interrupt entry rtos_int_enter (portasm.S, in IRAM) calls pxPortGetCoprocArea when the hardware-loop state
 * is dirty, and so does hwlp_restore_if_used; components/freertos/linker.lf does not place that function in IRAM, so
 * with CONFIG_FREERTOS_IN_IRAM=n it is fetched from flash. An interrupt on the core parked by a flash operation (the
 * cache off) then makes the cache error seen at boot on the device (trial install 12, 2026-11-11: core 1, while the
 * storage task read flash; driver-audit-1c C22). FREERTOS_IN_IRAM would move only the text, not the size tables below.
 * -Wl,--wrap=pxPortGetCoprocArea (main/CMakeLists.txt) sends portasm.S's calls here; port.c's own calls are from tasks.
 */
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "esp_attr.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "riscv/rvruntime-frames.h"
#include "esp_private/panic_internal.h"

#if !(ESP_IDF_VERSION_MAJOR == 6 && ESP_IDF_VERSION_MINOR == 1)
#error "s31_coproc.c copies ESP-IDF v6.1's pxPortGetCoprocArea: compare it with the new ESP-IDF before building"
#endif

#if SOC_CPU_COPROC_NUM > 0
#define STACKPTR_ALIGN_DOWN(n, ptr) ((ptr) & (~((n)-1)))
extern void xt_unhandled_exception(void *frame);

static DRAM_ATTR const uint32_t coproc_sa_sizes[] = {RV_COPROC0_SIZE, RV_COPROC1_SIZE, RV_COPROC2_SIZE};
static DRAM_ATTR const uint32_t coproc_sa_align[] = {RV_COPROC0_ALIGN, RV_COPROC1_ALIGN, RV_COPROC2_ALIGN};

RvCoprocSaveArea *__wrap_pxPortGetCoprocArea(StaticTask_t *task, bool allocate, int coproc);
RvCoprocSaveArea *IRAM_ATTR __wrap_pxPortGetCoprocArea(StaticTask_t *task, bool allocate, int coproc)
{
    /* the original asserts coproc < SOC_CPU_COPROC_NUM; its callers in portasm.S pass a constant index */
    const UBaseType_t bottomstack = (UBaseType_t)task->pxDummy8;
    RvCoprocSaveArea *sa = (RvCoprocSaveArea *)STACKPTR_ALIGN_DOWN(16, bottomstack - sizeof(RvCoprocSaveArea));
    if (sa->sa_allocator == 0) {
        sa->sa_tcbstack = task->pxDummy6;
        sa->sa_allocator = (UBaseType_t)task->pxDummy6;
    }
    if (allocate && sa->sa_coprocs[coproc] == NULL) {
        sa->sa_allocator = (sa->sa_allocator + coproc_sa_align[coproc] - 1) & ~(coproc_sa_align[coproc] - 1);
        sa->sa_coprocs[coproc] = (void *)(sa->sa_allocator);
        sa->sa_allocator += coproc_sa_sizes[coproc];
        task->pxDummy6 = (void *)(sa->sa_allocator);
        void *task_sp = task->pxDummy1;
        if (task_sp <= task->pxDummy6) {
            g_panic_abort = true;
            g_panic_abort_details = (char *)"ERROR: Stack overflow while saving coprocessor context!\n";
            xt_unhandled_exception(task_sp);
        }
    }
    return sa;
}
#endif
