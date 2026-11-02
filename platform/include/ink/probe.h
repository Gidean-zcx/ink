/*
 * ink probes — portable instrumentation for latency and behaviour budgets.
 *
 * System code reports what matters to the user ("touch to first visible
 * feedback", "page turn") through these calls. In the simulator they feed the
 * metrics report, the trace and the CI budgets; on the device they go to the
 * log at debug level (or compile out). Keep names stable: budgets refer to them.
 *
 * Naming: "<area>.<what>_<unit>", e.g. "ui.touch_feedback_ms", "reader.page_turn_ms".
 */
#ifndef INK_PROBE_H
#define INK_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Record one sample of a metric (the report shows count, p50, p95, max). */
void ink_probe_value(const char *metric, double value);
/** Increment a counter metric by 1. */
void ink_probe_count(const char *metric);
/** A named instant on the timeline (trace marker). */
void ink_probe_mark(const char *name);

#ifdef __cplusplus
}
#endif
#endif
