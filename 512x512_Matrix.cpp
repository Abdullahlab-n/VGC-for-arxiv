// === VGC 2.5 PPE Matrix Benchmark (Single-Core, 512x512 Matrix Multiply) ===
// Compile:
// g++ -O3 -march=native -std=c++17 ppe_matrix_512.cpp -lpsapi -o matrix512.exe

#include <windows.h>
#include <psapi.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>

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

// =======================================================================
// Matrix Multiplication (Cache-Friendly, Single-Core PPE Benchmark)
// =======================================================================

short compute_checksum(const std::vector<std::vector<float>>& M) {
    short acc = 0;
    for (const auto& row : M)
        for (float x : row)
            acc += static_cast<short>(static_cast<int>(x) % 32767);
    return acc;
}

int main() {
    const int N = 512;  // matrix size (512x512)

    sys::pin_to_core_and_boost(0);

    std::cout << "=== VGC 2.5 PPE Matrix Benchmark (Single-Core, 512x512) ===\n";
    std::cout << "Matrix Size: " << N << " x " << N << "\n\n";

    // Random generator
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(0.0f, 10.0f);

    // Allocate matrices
    std::vector<std::vector<float>> A(N, std::vector<float>(N));
    std::vector<std::vector<float>> B(N, std::vector<float>(N));
    std::vector<std::vector<float>> C(N, std::vector<float>(N, 0.0f));

    // Fill with random numbers
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            A[i][j] = dist(rng);
            B[i][j] = dist(rng);
        }

    std::size_t mem_before = sys::working_set_kb();
    sys::Timer T;

    // Matrix multiply (cache-friendly i-k-j loop)
    for (int i = 0; i < N; i++) {
        for (int k = 0; k < N; k++) {
            float aik = A[i][k];
            for (int j = 0; j < N; j++) {
                C[i][j] += aik * B[k][j];
            }
        }
    }

    double ms = T.ms();
    std::size_t mem_after = sys::working_set_kb();

    short checksum = compute_checksum(C);

    std::cout << "[Matrix Execution]\n";
    std::cout << "Time: " << std::fixed << std::setprecision(6) << ms << " ms\n";
    std::cout << "Checksum: " << checksum << "\n";
    std::cout << "Memory Before: " << mem_before << " KB\n";
    std::cout << "Memory After : " << mem_after << " KB\n";
    std::cout << "Memory Delta : " << (mem_after - mem_before) << " KB\n";
    std::cout << "===============================================================\n";

    return 0;
}
