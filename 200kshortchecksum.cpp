// === VGC 2.5 PPE Loop Benchmark (Single-Core, Variable N, Short Checksum) ===
// Compile: g++ -O3 -march=native -std=c++17 vgc_200k.cpp -lpsapi -o vgc_200k.exe

#include <windows.h>
#include <psapi.h>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace sys {
    struct Timer {
        using Clock = std::chrono::steady_clock;
        Clock::time_point start;
        Timer() : start(Clock::now()) {}
        double ms() const {
            using dur = std::chrono::duration<double, std::milli>;
            return std::chrono::duration_cast<dur>(Clock::now() - start).count();
        }
    };

    std::size_t working_set_kb() {
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
            return static_cast<std::size_t>(pmc.WorkingSetSize / 1024);
        return 0;
    }

    void pin_to_core_and_boost(unsigned core_index = 0) {
        DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << core_index);
        SetThreadAffinityMask(GetCurrentThread(), mask);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }
}

// ============================================================
// Single-core loop (optimized for small scale)
// ============================================================
short loop_chunk(std::size_t begin, std::size_t end) {
    short acc = 0;
    for (std::size_t i = begin; i < end; ++i)
        acc += static_cast<short>((2 * i + 1) % 32767);  // stays inside short
    return acc;
}

int main() {
    const std::size_t N = 200000;  // change for 400k later
    sys::pin_to_core_and_boost(0);

    std::cout << "=== VGC 2.5 PPE Loop Benchmark (Single-Core, "
              << N << " Loops, Short Checksum) ===\n";
    std::cout << "Workload N: " << N << "\n";
    std::cout << "Partitions: 1 (Single-Core)\n\n";

    std::size_t mem_before = sys::working_set_kb();

    sys::Timer T;
    short checksum = loop_chunk(0, N);
    double ms = T.ms();

    std::size_t mem_after = sys::working_set_kb();

    std::cout << "[Loop Partition]\n";
    std::cout << "Time: " << std::fixed << std::setprecision(6) << ms << " ms\n";
    std::cout << "Checksum: " << checksum << "\n";
    std::cout << "Memory Before: " << mem_before << " KB\n";
    std::cout << "Memory After : " << mem_after << " KB\n";
    std::cout << "Memory Delta : " << (mem_after - mem_before) << " KB\n";
    std::cout << "===============================================================\n";
}
