//  █████╗ ██████╗ ███╗   ███╗██╗      ██████╗
// ██╔══██╗╚════██╗████╗ ████║██║     ██╔════╝
// ███████║ █████╔╝██╔████╔██║██║     ██║     
// ██╔══██║██╔═══╝ ██║╚██╔╝██║██║     ██║     
// ██║  ██║███████╗██║ ╚═╝ ██║███████╗╚██████╗
// ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝╚══════╝ ╚═════╝
// AVX2 Memory Latency Checker

#include <stdio.h>
#include <stdlib.h>
#include <immintrin.h> // For AVX2 instructions
#include <string.h>     // For strcmp.h
#include <intrin.h>     // For __cpuid and __rdtsc
#include <windows.h>    // For CPU core binding

#define ARRAY_SIZE (32 * 1024 * 1024) // Array size
#define ITERATIONS 100                // 100 iterations per set

// Function to retrieve the CPU frequency using CPUID instruction
unsigned int get_cpu_frequency() {
    int cpu_info[4];  // Array to store the CPUID result
    __cpuid(cpu_info, 0x16);  // Get the CPU frequency info from CPUID
    return cpu_info[1] * 1000; // Frequency in MHz
}

// Function to bind the program to specific cores (core 1 and core 2)
void bind_to_cores() {
    DWORD_PTR mask = (1 << 2);  // Core 2
    SetThreadAffinityMask(GetCurrentThread(), mask);  // Set affinity
}

// Function to set the thread to real-time priority
void set_real_time_priority() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

// Function to measure latency using RDTSC (Read Time-Stamp Counter)
double measure_latency_avx2(uintptr_t *array, size_t size, const char *operation, unsigned int freq) {
    __m256i *ptr = (__m256i *)array;
    volatile __m256i dummy = _mm256_setzero_si256();  // Prevent optimization
    unsigned long long start, end;
    double total_time_ns = 0.0;

    for (size_t i = 0; i < ITERATIONS; i++) {
        // Read Time-Stamp Counter at the start
        start = __rdtsc();

        // Loop through the entire array
        for (size_t j = 0; j < size / sizeof(__m256i); j++) {
            switch (operation[0]) {
                case 'r': // Read
                    dummy = _mm256_load_si256(&ptr[j]);
                    if (operation[1] == 'r') { // Read-Read
                        dummy = _mm256_load_si256(&ptr[j]);
                    } else if (operation[1] == 'w') { // Read-Write
                        _mm256_store_si256(&ptr[j], dummy);
                    }
                    break;
                case 'w': // Write
                    _mm256_store_si256(&ptr[j], dummy);
                    if (operation[1] == 'r') { // Write-Read
                        dummy = _mm256_load_si256(&ptr[j]);
                    } else if (operation[1] == 'w') { // Write-Write
                        _mm256_store_si256(&ptr[j], dummy);
                    }
                    break;
            }
        }

        // Read Time-Stamp Counter at the end
        end = __rdtsc();

        // Calculate elapsed time in nanoseconds using CPU frequency (in MHz)
        double elapsed_ns = (double)(end - start) * 1000.0 / (double)freq; // Convert to ns
        total_time_ns += elapsed_ns; // Accumulate the time
    }

    return total_time_ns / ITERATIONS; // Return average latency in nanoseconds
}

// Function to calculate bandwidth
double calculate_bandwidth(size_t size, double total_time_ns) {
    return (1024.0 * 1024.0 * 1024.0) / ((size) * (total_time_ns) / (5)); // Bandwidth in GB/s
}

int main() {
    system("cls");
    // Setup console handle and cursor position
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD cursor_start;
    cursor_start.X = 0;
    cursor_start.Y = 2; // Row after the header
    


    uintptr_t *array = (uintptr_t *)_aligned_malloc(ARRAY_SIZE, 32);  // Align for AVX2
    if (!array) {
        perror("Memory allocation failed");
        return 1;
    }

    // Retrieve CPU frequency (in MHz)
    float cpu_freq = get_cpu_frequency();
    printf("CPU Frequency: %.1f GHz\n", cpu_freq / 1000000);

    // Initialize the array (pointer chasing pattern)
    size_t array_length = ARRAY_SIZE / sizeof(uintptr_t);
    for (size_t i = 0; i < array_length; i++) {
        array[i] = (uintptr_t)&array[(i + 1) % array_length]; // Pointer chasing
    }

    // Store results
    double avx2_latencies[6] = {0};
    double avx2_bandwidths[6] = {0}; // Store bandwidths for each operation

    // Bind the program to specific cores
    bind_to_cores();
    set_real_time_priority();

    // Print the header once
    printf("%-16s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n", "Type", "R", "W", "R->R", "R->W", "W->R", "W->W", "Cycles", "Mean");

    // Continuously measure latencies and update results
    unsigned long long run_count = 0;
    while (1) {
        double avx2_latencies_temp[6] = {0};
        double avx2_bandwidths_temp[6] = {0};

        // Measure latencies for each operation
        const char *operations[] = {"rr", "rw", "wr", "ww", "r", "w"};
        for (int i = 0; i < 6; i++) {
            avx2_latencies_temp[i] = measure_latency_avx2(array, array_length, operations[i], cpu_freq);
            avx2_bandwidths_temp[i] = calculate_bandwidth(array_length, avx2_latencies_temp[i]);
        }

        // Update the averages with the current run's results
        for (int i = 0; i < 6; i++) {
            avx2_latencies[i] = (avx2_latencies[i] * run_count + avx2_latencies_temp[i]) / (run_count + 1);
            avx2_bandwidths[i] = (avx2_bandwidths[i] * run_count + avx2_bandwidths_temp[i]) / (run_count + 1);
        }

        // Increment the run count
        run_count++;

        
        // Move cursor up 2 rows and print both latency and bandwidth on the same position
        printf("\033[A\033[A"); // ANSI escape sequence to move the cursor up 2 rows

        SetConsoleCursorPosition(hConsole, cursor_start); // Reset cursor to start position
        // Print Latency row
        printf("%-16s %-10.2f %-10.2f %-10.2f %-10.2f %-10.2f %-10.2f %-10lld %.2f\n", 
            "Latency (ns)", 
            avx2_latencies[4], avx2_latencies[5], avx2_latencies[0], avx2_latencies[1], 
            avx2_latencies[2], avx2_latencies[3], run_count, (avx2_latencies[4] + avx2_latencies[5] + avx2_latencies[0] + avx2_latencies[1] + avx2_latencies[2] + avx2_latencies[3]) / 6.0);

        // Print Bandwidth row
        printf("%-16s %-10.2f %-10.2f %-10.2f %-10.2f %-10.2f %-21.2f %.2f\n", 
            "Bandwidth (GB/s)", 
            avx2_bandwidths[4], avx2_bandwidths[5], avx2_bandwidths[0], avx2_bandwidths[1], 
            avx2_bandwidths[2], avx2_bandwidths[3], (avx2_bandwidths[4] + avx2_bandwidths[5] + avx2_bandwidths[0] + avx2_bandwidths[1] + avx2_bandwidths[2] + avx2_bandwidths[3]) / 6.0);

        fflush(stdout);
        Sleep(1);  // Sleep to let the display refresh
    }

    // Clean up
    _aligned_free(array);
    return 0;
}
