/*
 * vtk — virtual-time kernel of the ink simulator.
 *
 * Runs real C tasks (host threads) under a deterministic discrete-event
 * scheduler that models the target's cores, fixed-priority preemption, CPU
 * cost and flash-induced stalls. Exactly one host thread executes simulated
 * code at any moment (a "baton" is passed), so results do not depend on the
 * host's thread scheduling. Idle virtual time is skipped instantly.
 *
 * Context rules (mirroring an RTOS):
 *  - task context: may block (vtk_block_*), consume CPU, call any API.
 *  - event context (callbacks registered with vtk_event_at): like an ISR —
 *    runs with the kernel lock held; may only call vtk_isr_* functions.
 */
#ifndef INK_VTK_H
#define INK_VTK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VTK_MAX_TASKS 96
#define VTK_MAX_CORES 4
#define VTK_TIME_INF UINT64_MAX

typedef enum {
    VTK_CPU_ANNOTATED = 0, /* deterministic: only annotated costs + fixed per-call overheads */
    VTK_CPU_MEASURED = 1,  /* host thread CPU time x scale + annotated costs */
} vtk_cpu_mode_t;

typedef struct {
    int ncores;
    vtk_cpu_mode_t cpu_mode;
    double host_to_target;        /* MEASURED: target ns per host ns of CPU */
    uint64_t sched_overhead_ns;   /* charged per scheduling point (kernel call) */
    uint64_t ctx_switch_ns;       /* charged when a core switches tasks */
    uint64_t long_burst_warn_ns;  /* warn when a task runs this long without a scheduling point */
    double host_stack_ratio;      /* host stack bytes per target stack byte (64-bit host vs RV32) */
    uint64_t end_time_ns;         /* stop at this virtual time (VTK_TIME_INF = never) */
    bool realtime;                /* pace virtual time to wall clock (interactive) */
    double realtime_speed;        /* 1.0 = real time */
} vtk_config_t;

typedef struct vtk_task vtk_task_t;
typedef void (*vtk_task_fn)(void *arg);
typedef void (*vtk_event_fn)(void *arg);

typedef enum {
    VTK_ST_READY = 0,
    VTK_ST_RUNNING,
    VTK_ST_BLOCKED,
    VTK_ST_DONE,
} vtk_state_t;

enum { VTK_TASK_IRAM_SAFE = 1u << 0 };

typedef struct {
    char name[32];
    int priority;
    int core;
    vtk_state_t state;
    uint64_t cpu_ns;            /* target CPU time consumed */
    uint64_t stall_ns;          /* time spent frozen by flash operations */
    uint64_t max_burst_ns;      /* longest run without a scheduling point */
    uint64_t preempt_delay_max_ns; /* worst observed delay between becoming ready and running */
    uint32_t stack_target_bytes;
    uint32_t stack_used_est_bytes; /* estimated target stack high-water mark */
    uint64_t switches;
} vtk_task_info_t;

/* ---- lifecycle ---- */
void vtk_init(const vtk_config_t *cfg);
const vtk_config_t *vtk_config(void);
vtk_task_t *vtk_task_create(const char *name, vtk_task_fn fn, void *arg, int priority, int core,
                            uint32_t flags, uint32_t stack_target_bytes);
/** Create a tiny probe task whose stack use is the host baseline for stack estimates. */
void vtk_calibrate_stack(void);
/** Run until no work remains, end time is reached, or vtk_stop(). Call from the main thread. */
void vtk_run(void);
/** Request stop (any context). */
void vtk_stop(void);
bool vtk_stopping(void);

/* ---- time & self ---- */
uint64_t vtk_now(void);          /* current virtual time in ns (of the calling task's core) */
vtk_task_t *vtk_self(void);
const char *vtk_task_name(const vtk_task_t *t);
int vtk_task_count(void);
void vtk_task_info(int index, vtk_task_info_t *out);

/* ---- task-context primitives ---- */
void vtk_sched_point(void);                 /* charge CPU and allow preemption */
void vtk_consume_ns(uint64_t ns);           /* annotated CPU work */
void vtk_sleep_ns(uint64_t ns);
void vtk_yield(void);
void vtk_task_exit(void) __attribute__((noreturn));
void vtk_set_priority(vtk_task_t *t, int prio); /* used for priority inheritance */
int vtk_priority(const vtk_task_t *t);
int vtk_base_priority(const vtk_task_t *t);

/* Generic wait queues for building sync objects (queues, mutexes, semaphores). */
typedef struct vtk_waitq {
    vtk_task_t *head;
} vtk_waitq_t;

void vtk_lock(void);
void vtk_unlock(void);
/** Block on wq (kernel lock must be held; it is held again on return).
 *  Returns true if woken, false on timeout. timeout_ns = VTK_TIME_INF for none. */
bool vtk_block_locked(vtk_waitq_t *wq, uint64_t timeout_ns);
/** Wake the highest-priority waiter (lock held). Returns the task or NULL. */
vtk_task_t *vtk_wake_one_locked(vtk_waitq_t *wq);
int vtk_wake_all_locked(vtk_waitq_t *wq);
/** Highest current priority among waiters, -1 if none (lock held). */
int vtk_waitq_max_priority(const vtk_waitq_t *wq);

/* ---- events (ISR-like context) ---- */
typedef uint64_t vtk_event_id_t;
vtk_event_id_t vtk_event_at(uint64_t time_ns, vtk_event_fn fn, void *arg);
/** Schedule relative to the caller's current virtual time. */
vtk_event_id_t vtk_event_after(uint64_t delay_ns, vtk_event_fn fn, void *arg);
void vtk_event_cancel(vtk_event_id_t id);
/** Inside event context: current event time. */
uint64_t vtk_isr_now(void);
vtk_task_t *vtk_isr_wake_one(vtk_waitq_t *wq);

/* ---- flash stall model ----
 * While internal flash is erased/programmed, CPU caches are off: every task
 * without VTK_TASK_IRAM_SAFE is frozen on all cores until the operation ends. */
void vtk_stall_begin_locked(uint64_t until_ns);
uint64_t vtk_stall_total_ns(void);

/* ---- realtime/external input ---- */
/** Thread-safe from ANY host thread (e.g. an SDL or network thread): run fn in event
 *  context at the current virtual time. Only meaningful in realtime mode. */
void vtk_inject_external(vtk_event_fn fn, void *arg);

/* ---- diagnostics ---- */
typedef void (*vtk_warn_fn)(const char *msg);
void vtk_set_warn_sink(vtk_warn_fn fn);

/** Called (kernel lock held) whenever a task starts or stops running on a core.
 *  `running` = true at start. Used for traces and per-core load. */
typedef void (*vtk_switch_hook_fn)(int core, const vtk_task_t *t, bool running, uint64_t t_ns);
void vtk_set_switch_hook(vtk_switch_hook_fn fn);
/** Busy time of a core so far (task execution incl. context switches). */
uint64_t vtk_core_busy_ns(int core);
int vtk_task_index(const vtk_task_t *t);
/** True while executing in event (ISR-like) context. */
bool vtk_in_isr(void);

#endif
