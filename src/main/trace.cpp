// See include/trace.h. Only used when RecompiledFuncs is generated with trace_mode = true.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include <mutex>
#include <thread>

#include "trace.h"

namespace {
    constexpr int max_depth = 256;
    constexpr int max_threads = 32;
    constexpr int frames_printed = 16;

    struct TraceStack {
        const char* names[max_depth];
        std::atomic<int> depth{0};
        std::atomic<uint64_t> calls{0};
        uint64_t last_calls = 0;
    };

    TraceStack stacks[max_threads];
    std::atomic<int> stack_count{0};
    thread_local TraceStack* this_stack = nullptr;
    std::once_flag watchdog_started;

    // Per-function call counters: lock-free open addressing keyed by the (static) __func__ pointer.
    constexpr size_t counter_slots = 8192;
    struct Counter {
        std::atomic<const char*> name{nullptr};
        std::atomic<uint64_t> calls{0};
        uint64_t last_calls = 0;
    };
    Counter counters[counter_slots];

    void count_call(const char* name) {
        size_t i = (reinterpret_cast<uintptr_t>(name) >> 3) % counter_slots;
        for (size_t probe = 0; probe < counter_slots; probe++, i = (i + 1) % counter_slots) {
            const char* cur = counters[i].name.load(std::memory_order_relaxed);
            if (cur == nullptr) {
                const char* expected = nullptr;
                if (!counters[i].name.compare_exchange_strong(expected, name) && expected != name) {
                    continue;
                }
                cur = name;
            }
            if (cur == name) {
                counters[i].calls.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
    }

    void print_counters() {
        std::vector<std::pair<uint64_t, const char*>> deltas;
        for (Counter& c : counters) {
            const char* name = c.name.load();
            if (name == nullptr) {
                continue;
            }
            uint64_t calls = c.calls.load();
            if (calls != c.last_calls) {
                deltas.emplace_back(calls - c.last_calls, name);
            }
            c.last_calls = calls;
        }
        std::sort(deltas.begin(), deltas.end(), [](auto& a, auto& b) { return a.first > b.first; });
        fprintf(stderr, "[trace] %zu distinct functions called in last 3s, top:\n", deltas.size());
        for (size_t i = 0; i < deltas.size() && i < 12; i++) {
            fprintf(stderr, "[trace]     %8llu %s\n", (unsigned long long)deltas[i].first, deltas[i].second);
        }
        // Scheduler functions that start RSP tasks and swap framebuffers.
        for (const char* watched : { "func_80036BF4", "func_80036B10", "func_8003683C", "func_8003693C", "func_80001AAC", "func_800038FC" }) {
            uint64_t n = 0;
            for (auto& d : deltas) {
                if (strcmp(d.second, watched) == 0) {
                    n = d.first;
                }
            }
            fprintf(stderr, "[trace]     watch %s: %llu\n", watched, (unsigned long long)n);
        }
    }

    void watchdog() {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            print_counters();
            int n = std::min(stack_count.load(), max_threads);
            fprintf(stderr, "[trace] ---- %d thread(s)\n", n);
            for (int i = 0; i < n; i++) {
                TraceStack& s = stacks[i];
                uint64_t calls = s.calls.load();
                int d = std::min(s.depth.load(), max_depth);
                fprintf(stderr, "[trace] thread %d: depth %d, %llu calls in last 3s\n", i, d, (unsigned long long)(calls - s.last_calls));
                s.last_calls = calls;
                for (int f = d - 1; f >= 0 && f >= d - frames_printed; f--) {
                    fprintf(stderr, "[trace]     %s\n", s.names[f] ? s.names[f] : "?");
                }
            }
        }
    }
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// On a native crash, print the recompiled call stack of the faulting thread.
static LONG WINAPI crash_handler(EXCEPTION_POINTERS* info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;
    if ((code & 0xC0000000) != 0xC0000000) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    static std::atomic<bool> reported{false};
    if (!reported.exchange(true)) {
        fprintf(stderr, "[crash] exception 0x%08lX\n", (unsigned long)code);
        if (this_stack != nullptr) {
            int d = std::min(this_stack->depth.load(), max_depth);
            for (int f = d - 1; f >= 0 && f >= d - 30; f--) {
                fprintf(stderr, "[crash]     %s\n", this_stack->names[f] ? this_stack->names[f] : "?");
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

extern "C" void winks_trace_enter(const char* name) {
    if (this_stack == nullptr) {
        std::call_once(watchdog_started, [] {
            std::thread(watchdog).detach();
#ifdef _WIN32
            AddVectoredExceptionHandler(1, crash_handler);
#endif
        });
        int idx = std::min(stack_count.fetch_add(1), max_threads - 1);
        this_stack = &stacks[idx];
    }
    int d = this_stack->depth.load(std::memory_order_relaxed);
    if (d >= 0 && d < max_depth) {
        this_stack->names[d] = name;
    }
    this_stack->depth.store(d + 1, std::memory_order_relaxed);
    this_stack->calls.fetch_add(1, std::memory_order_relaxed);
    count_call(name);
}

extern "C" void winks_trace_leave() {
    int d = this_stack->depth.load(std::memory_order_relaxed);
    if (d > 0) {
        this_stack->depth.store(d - 1, std::memory_order_relaxed);
    }
}
