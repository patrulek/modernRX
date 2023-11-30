#pragma once

#include <atomic>
#include <print>
#include <vector>

#ifdef TRACE
constexpr bool Trace_Enabled{ true };
#else
constexpr bool Trace_Enabled{ false };
#endif


enum class TraceEvent {
    Generate,
    Compile,
    Execute,
    HashAndFill,
    Max,
};

constexpr std::array<std::string_view, static_cast<size_t>(TraceEvent::Max)> Trace_Event_Names {
    "Generate",
    "Compile",
    "Execute",
    "HashAndFill",
};

struct TraceData {
    std::atomic<size_t> clock_sum{ 0 };
    std::atomic<size_t> clock_samples{ 0 };
    std::atomic<size_t> clock_min{ std::numeric_limits<size_t>::max() };
    std::atomic<size_t> clock_max{ 0 };
};

inline std::array<TraceData, static_cast<size_t>(TraceEvent::Max)> trace_data{};

template<TraceEvent evt>
struct Trace {
    size_t clock_start{ 0 };

    [[maybe_unused]] explicit Trace() {
        if constexpr (!Trace_Enabled) {
            return;
        }

        clock_start = __rdtsc();
    }

    Trace(const Trace&) = delete;
    Trace& operator=(const Trace&) = delete;
    Trace(Trace&&) = delete;
    Trace& operator=(Trace&&) = delete;

    ~Trace() {
        if constexpr (!Trace_Enabled) {
            return;
        }

        const uint64_t clock_stop{ __rdtsc() };
        auto& td{ trace_data[static_cast<size_t>(evt)] };
        const uint64_t clock_diff{ clock_stop - clock_start };
        td.clock_sum.fetch_add(clock_diff, std::memory_order_relaxed);
        td.clock_samples.fetch_add(1, std::memory_order_relaxed);
        td.clock_min.store(std::min(td.clock_min.load(std::memory_order_relaxed), clock_diff), std::memory_order_relaxed);
        td.clock_max.store(std::max(td.clock_max.load(std::memory_order_relaxed), clock_diff), std::memory_order_relaxed);
        
    }
};


struct TraceResults {
    [[maybe_unused]] TraceResults() = default;

    TraceResults(const TraceResults&) = delete;
    TraceResults& operator=(const TraceResults&) = delete;
    TraceResults(TraceResults&&) = delete;
    TraceResults& operator=(TraceResults&&) = delete;

     ~TraceResults() {
        if constexpr (!Trace_Enabled) {
            return;
        }

        size_t total_clocks{ 0 };
        for (size_t i = 0; i < static_cast<size_t>(TraceEvent::Max); ++i) {
            auto& td{ trace_data[i] };

            if (i == static_cast<size_t>(TraceEvent::HashAndFill)) {
                td.clock_samples.store(td.clock_samples.load() / 8);
                td.clock_sum.store(td.clock_sum.load() / 8);
            }

            total_clocks += td.clock_sum;
        }

        std::println("\nTrace results:");
        for (size_t i = 0; i < static_cast<size_t>(TraceEvent::Max); ++i) {
            const auto& td{ trace_data[i] };
            if (td.clock_samples == 0) {
                continue;
            }

            std::println("Trace event ({}): {} samples, clocks (min: {}, max: {}, avg: {}), {:.2f}% of total time",
                Trace_Event_Names[i], td.clock_samples.load(), td.clock_min.load(), td.clock_max.load(),
                (td.clock_sum.load() / td.clock_samples.load()), 100.0 * td.clock_sum.load() / total_clocks);
        }
    }
};