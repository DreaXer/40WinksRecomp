// Debug call tracing for the recompiled game code (enabled by "trace_mode = true" in 40winks.toml).
// N64Recomp then emits TRACE_ENTRY()/TRACE_RETURN() at the start/end of every recompiled function.
// Implemented in src/main/trace.cpp: per-thread call stacks plus a watchdog that prints them every few seconds.
#ifndef WINKS_TRACE_H
#define WINKS_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif
void winks_trace_enter(const char *name);
void winks_trace_leave(void);
#ifdef __cplusplus
}
#endif

#define TRACE_ENTRY() winks_trace_enter(__func__);
#define TRACE_RETURN() winks_trace_leave();

#endif
