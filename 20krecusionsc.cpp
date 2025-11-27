// === VGC 2.5 PPE Recursion Benchmark (Single-Core, 20K Steps, Short Checksum) ===
// Compile: g++ -O3 -march=native -std=c++17 recursion_20k.cpp -lpsapi -o recursion_20k.exe

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

    void pin_to_core_and_boost(unsigned c = 0) {
        DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << c);
        SetThreadAffinityMask(GetCurrentThread(), mask);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }
}

// Safe Recursion: Depth 1000. Accumulator pattern.

short recursive_chunk(int depth, int limit, short acc) {
    if (depth == limit) return acc;
    return recursive_chunk(depth + 1, limit,
                           acc + static_cast<short>((depth * 3 + 1) % 32767));
}

short recursive_driver(int total_steps, int chunk_size = 1000) {
    short acc = 0;
    int done = 0;
    while (done < total_steps) {
        acc += recursive_chunk(0, chunk_size, 0);
        done += chunk_size;
    }
    return acc;
}

int main() {
    const int TOTAL = 20000;   // <-- 20K recurion 
    sys::pin_to_core_and_boost(0);

    std::cout << "=== VGC 2.5 PPE Recursion Benchmark (Single-Core, 20K Steps) ===\n";
    std::cout << "Logical Steps: " << TOTAL << "\n";
    std::cout << "Recursion Depth per Chunk: 1000\n\n";

    std::size_t mem_before = sys::working_set_kb();

    sys::Timer T;
    short checksum = recursive_driver(TOTAL);
    double ms = T.ms();

    std::size_t mem_after = sys::working_set_kb();

    std::cout << "[Recursion Execution]\n";
    std::cout << "Time: " << std::fixed << std::setprecision(6) << ms << " ms\n";
    std::cout << "Checksum: " << checksum << "\n";
    std::cout << "Memory Before: " << mem_before << " KB\n";
    std::cout << "Memory After : " << mem_after << " KB\n";
    std::cout << "Memory Delta : " << (mem_after - mem_before) << " KB\n";
    std::cout << "===============================================================\n";
}

