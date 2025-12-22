// === VGC 2.5 PPE Deep Recursion Benchmark (Single-Core, Variable N, Short Checksum) === //
// Compiler                                                                              //
// g++ -O3 -march=native -std=c++17 ppe_deeprecursion.cpp -lpsapi -o deeprec.exe        //

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

    void pin_to_core_and_boost(unsigned core = 0) {
        DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << core);
        SetThreadAffinityMask(GetCurrentThread(), mask);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }
}

// =====================================================================  //
// Deep Recursion: depth = 4000 (default)                                //
// This simulates heavy stack usage without overflowing.                //
// =================================================================== //
short deep_recurse(int depth, int limit, short acc) {
    if (depth == limit) 
        return acc;

    return deep_recurse(
        depth + 1,
        limit,
        acc + static_cast<short>((depth * 7 + 3) % 32767)
    );
}

// Wrapper that chunks deep recursion safely.
short deep_driver(int total_steps, int chunk_depth = 4000) {
    short acc = 0;
    int done = 0;

    while (done < total_steps) {
        acc += deep_recurse(0, chunk_depth, 0);
        done += chunk_depth;
    }
    return acc;
}

int main() {
    // === CHANGE THIS for 10K, 20K, 40K deep recursion ===
    const int TOTAL = 40000;    // example: 40K deep recursion steps
    const int DEPTH = 4000;     // deep recursive depth

    sys::pin_to_core_and_boost(0);

    std::cout << "=== VGC 2.5 PPE Deep Recursion Benchmark (Single-Core) ===\n";
    std::cout << "Logical Steps: " << TOTAL << "\n";
    std::cout << "Recursion Depth per Chunk: " << DEPTH << "\n\n";

    std::size_t mem_before = sys::working_set_kb();

    sys::Timer T;
    short checksum = deep_driver(TOTAL, DEPTH);
    double ms = T.ms();

    std::size_t mem_after = sys::working_set_kb();

    std::cout << "[Deep Recursion Execution]\n";
    std::cout << "Time: " << std::fixed << std::setprecision(6) << ms << " ms\n";
    std::cout << "Checksum: " << checksum << "\n";
    std::cout << "Memory Before: " << mem_before << " KB\n";
    std::cout << "Memory After : " << mem_after << " KB\n";
    std::cout << "Memory Delta : " << (mem_after - mem_before) << " KB\n";
    std::cout << "===============================================================\n";

    return 0;
}
