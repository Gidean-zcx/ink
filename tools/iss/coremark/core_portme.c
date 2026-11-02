/*
 * CoreMark port for the iss simulator: time = simulated core cycles; the timed
 * section is also recorded as the measured region "coremark" (units =
 * iterations), which is what the calibration reads.
 *
 * Copyright 2018 Embedded Microprocessor Benchmark Consortium (EEMBC),
 * Apache License 2.0 (structure follows coremark/barebones/core_portme.c).
 */
#include "coremark.h"
#include "core_portme.h"
#include "iss_rt.h"

#ifndef ITERATIONS
#define ITERATIONS 50
#endif
#ifndef ISS_CPU_HZ
#define ISS_CPU_HZ 320000000u
#endif

#if VALIDATION_RUN
volatile ee_s32 seed1_volatile = 0x3415;
volatile ee_s32 seed2_volatile = 0x3415;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PERFORMANCE_RUN
volatile ee_s32 seed1_volatile = 0x0;
volatile ee_s32 seed2_volatile = 0x0;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PROFILE_RUN
volatile ee_s32 seed1_volatile = 0x8;
volatile ee_s32 seed2_volatile = 0x8;
volatile ee_s32 seed3_volatile = 0x8;
#endif
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;

static CORETIMETYPE start_time_val, stop_time_val;

void start_time(void) {
    start_time_val = (CORETIMETYPE)iss_cycles();
    iss_begin("coremark");
}

void stop_time(void) {
    iss_end((uint32_t)seed4_volatile);
    stop_time_val = (CORETIMETYPE)iss_cycles();
}

CORE_TICKS get_time(void) { return (CORE_TICKS)(stop_time_val - start_time_val); }

secs_ret time_in_secs(CORE_TICKS ticks) { return (secs_ret)ticks / (secs_ret)ISS_CPU_HZ; }

ee_u32 default_num_contexts = 1;

void portable_init(core_portable *p, int *argc, char *argv[]) {
    (void)argc;
    (void)argv;
    if (sizeof(ee_ptr_int) != sizeof(ee_u8 *)) ee_printf("ERROR! ee_ptr_int must hold a pointer\n");
    if (sizeof(ee_u32) != 4) ee_printf("ERROR! ee_u32 must be 32 bits\n");
    p->portable_id = 1;
}

void portable_fini(core_portable *p) { p->portable_id = 0; }
