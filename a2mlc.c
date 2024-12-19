//  █████╗ ██████╗ ███╗   ███╗██╗      ██████╗
// ██╔══██╗╚════██╗████╗ ████║██║     ██╔════╝
// ███████║ █████╔╝██╔████╔██║██║     ██║     
// ██╔══██║██╔═══╝ ██║╚██╔╝██║██║     ██║     
// ██║  ██║███████╗██║ ╚═╝ ██║███████╗╚██████╗
// ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝╚══════╝ ╚═════╝
// AVX2 Memory Latency Checker

#include <stdio.h>
#include <stdlib.h>
#include <immintrin.h>
#include <string.h>
#include <windows.h>

#define ARRAY_SIZE (32 * 1024 * 1024)
#define ITERATIONS 100

// Function to bind the program to specific cores (core 1 and core 2)
void bind_to_cores() {
    DWORD_PTR mask = (1 << 2);  // Core 2
    SetThreadAffinityMask(GetCurrentThread(), mask);
}

// Function to set the thread to real-time priority
void set_real_time_priority() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

// Function to measure latency using QueryPerformanceCounter
double measure_latency_avx2(uintptr_t *array, size_t size, const char *operation) {
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    
    __m256i *ptr = (__m256i *)array;
    volatile __m256i dummy = _mm256_setzero_si256();
    double total_time_ns = 0.0;

    for (size_t i = 0; i < ITERATIONS; i++) {
        QueryPerformanceCounter(&start);

        for (size_t j = 0; j < size / sizeof(__m256i); j++) {
            switch (operation[0]) {
                case 'r':
                    dummy = _mm256_load_si256(&ptr[j]);
                    if (operation[1] == 'r') {
                        dummy = _mm256_load_si256(&ptr[j]);
                    } else if (operation[1] == 'w') {
                        _mm256_store_si256(&ptr[j], dummy);
                    }
                    break;
                case 'w':
                    _mm256_store_si256(&ptr[j], dummy);
                    if (operation[1] == 'r') {
                        dummy = _mm256_load_si256(&ptr[j]);
                    } else if (operation[1] == 'w') {
                        _mm256_store_si256(&ptr[j], dummy);
                    }
                    break;
            }
        }

        QueryPerformanceCounter(&end);
        
        // Calculate elapsed time in nanoseconds
        double elapsed_ns = ((end.QuadPart - start.QuadPart) * 750000.0) / frequency.QuadPart;
        total_time_ns += elapsed_ns;
    }

    return total_time_ns / ITERATIONS;
}

// Function to calculate bandwidth
double calculate_bandwidth(size_t size, double total_time_ns) {
    return (1024.0 * 1024.0 * 1024.0) / ((size) * (total_time_ns) / (5));
}

int main() {
    system("cls");
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD cursor_start;
    cursor_start.X = 0;
    cursor_start.Y = 1;

    uintptr_t *array = (uintptr_t *)_aligned_malloc(ARRAY_SIZE, 32);
    if (!array) {
        perror("Memory allocation failed");
        return 1;
    }

    // Initialize the array (pointer chasing pattern)
    size_t array_length = ARRAY_SIZE / sizeof(uintptr_t);
    for (size_t i = 0; i < array_length; i++) {
        array[i] = (uintptr_t)&array[(i + 1) % array_length];
    }

    double avx2_latencies[6] = {0};
    double avx2_bandwidths[6] = {0};

    bind_to_cores();
    set_real_time_priority();

    printf("%-16s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n", 
           "Type", "R", "W", "R->R", "R->W", "W->R", "W->W", "Cycles", "Mean");

    unsigned long long run_count = 0;
    while (1) {
        double avx2_latencies_temp[6] = {0};
        double avx2_bandwidths_temp[6] = {0};

        const char *operations[] = {"rr", "rw", "wr", "ww", "r", "w"};
        for (int i = 0; i < 6; i++) {
            avx2_latencies_temp[i] = measure_latency_avx2(array, array_length, operations[i]);
            avx2_bandwidths_temp[i] = calculate_bandwidth(array_length, avx2_latencies_temp[i]);
        }

        for (int i = 0; i < 6; i++) {
            avx2_latencies[i] = (avx2_latencies[i] * run_count + avx2_latencies_temp[i]) / (run_count + 1);
            avx2_bandwidths[i] = (avx2_bandwidths[i] * run_count + avx2_bandwidths_temp[i]) / (run_count + 1);
        }

        run_count++;

        printf("\033[A\033[A");
        SetConsoleCursorPosition(hConsole, cursor_start);
        
        printf("%-16s %-10.2f %-10.2f %-10.2f %-10.2f %-10.2f %-10.2f %-10lld %.2f\n", 
            "Latency (ns)", 
            avx2_latencies[4], avx2_latencies[5], avx2_latencies[0], avx2_latencies[1], 
            avx2_latencies[2], avx2_latencies[3], run_count, 
            (avx2_latencies[4] + avx2_latencies[5] + avx2_latencies[0] + avx2_latencies[1] + 
             avx2_latencies[2] + avx2_latencies[3]) / 6.0);

        printf("%-16s %-10.2f %-10.2f %-10.2f %-10.2f %-10.2f %-21.2f %.2f\n", 
            "Bandwidth (GB/s)", 
            avx2_bandwidths[4], avx2_bandwidths[5], avx2_bandwidths[0], avx2_bandwidths[1], 
            avx2_bandwidths[2], avx2_bandwidths[3], 
            (avx2_bandwidths[4] + avx2_bandwidths[5] + avx2_bandwidths[0] + avx2_bandwidths[1] + 
             avx2_bandwidths[2] + avx2_bandwidths[3]) / 6.0);

        fflush(stdout);
        Sleep(1);
    }

    _aligned_free(array);
    return 0;
}
