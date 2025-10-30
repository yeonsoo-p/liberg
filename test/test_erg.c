#include <assert.h>
#include <erg.h>
#include <thread_pool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#define EPSILON 1e-9

/* Timing utility - returns time in nanoseconds */
static double get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)(counter.QuadPart * 1000000000.0) / frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000.0 + ts.tv_nsec;
#endif
}

void test_erg_basic(const char* erg_path) {
    printf("Testing basic ERG parsing...\n");

    ERG erg;
    erg_init(&erg, erg_path, NULL);  /* NULL = single-threaded */
    erg_parse(&erg);

    printf("  Loaded ERG file: %s\n", erg_path);
    printf("  Number of signals: %zu\n", erg.signal_count);
    printf("  Number of samples: %zu\n", erg.sample_count);
    printf("  Row size: %zu bytes\n", erg.row_size);
    printf("  Endianness: %s\n", erg.little_endian ? "Little-Endian" : "Big-Endian");

    // Basic sanity checks
    assert(erg.signal_count > 0);
    assert(erg.sample_count > 0);
    assert(erg.signals != NULL);

    printf("[OK] Basic ERG parsing test passed\n");

    erg_free(&erg);
}

void test_erg_signal_access(const char* erg_path) {
    printf("\nTesting signal access...\n");

    ERG erg;
    erg_init(&erg, erg_path, NULL);  /* NULL = single-threaded */
    erg_parse(&erg);

    // Test getting Time signal
    double* time_data = erg_get_signal_as_double(&erg, "Time");

    if (time_data) {
        printf("  Found 'Time' signal: %zu samples\n", erg.sample_count);

        printf("  Time range: %.3f to %.3f seconds\n",
               time_data[0], time_data[erg.sample_count - 1]);

        // Check that time is monotonically increasing
        for (size_t i = 1; i < erg.sample_count; i++) {
            assert(time_data[i] >= time_data[i - 1]);
        }

        free(time_data);
        printf("[OK] Time signal test passed\n");
    } else {
        printf("  'Time' signal not found, testing with first signal\n");
    }

    // Test signal info
    if (erg.signal_count > 0) {
        const char*      first_signal_name = erg.signals[0].name;
        const ERGSignal* sig_info          = erg_get_signal_info(&erg, first_signal_name);

        assert(sig_info != NULL);
        printf("  First signal: %s\n", sig_info->name);
        printf("    Type size: %zu bytes\n", sig_info->type_size);
        printf("    Unit: %s\n", sig_info->unit);
        printf("    Factor: %.6f\n", sig_info->factor);
        printf("    Offset: %.6f\n", sig_info->offset);

        printf("[OK] Signal info test passed\n");
    }

    // Test non-existent signal
    double* null_data = erg_get_signal_as_double(&erg, "NonExistentSignal123");
    assert(null_data == NULL);
    printf("[OK] Non-existent signal test passed\n");

    erg_free(&erg);
}

void test_erg_export_csv(const char* erg_path) {
    printf("\nExporting CSV file...\n");

    ERG erg;
    erg_init(&erg, erg_path, NULL);  /* NULL = single-threaded */
    erg_parse(&erg);

    // Signal names to export
    const char* signal_names[] = {"Time", "Car.ax", "Car.v", "Vhcl.tRoad"};
    size_t      num_signals    = sizeof(signal_names) / sizeof(signal_names[0]);

    // Get all signals as double arrays
    double* signals[4]    = {NULL, NULL, NULL, NULL};
    int     found_signals = 0;

    for (size_t i = 0; i < num_signals; i++) {
        signals[i] = erg_get_signal_as_double(&erg, signal_names[i]);
        if (signals[i]) {
            printf("  Found signal: %s (%zu samples)\n", signal_names[i], erg.sample_count);
            found_signals++;
        } else {
            printf("  Signal not found: %s\n", signal_names[i]);
        }
    }

    if (found_signals == 0) {
        printf("[WARNING] No signals found for CSV export\n");
        erg_free(&erg);
        return;
    }

    // Open CSV file for writing
    FILE* csv = fopen("result.csv", "w");
    if (!csv) {
        fprintf(stderr, "ERROR: Failed to create result.csv\n");
        for (size_t i = 0; i < num_signals; i++) {
            if (signals[i])
                free(signals[i]);
        }
        erg_free(&erg);
        return;
    }

    // Write CSV header
    fprintf(csv, "Time,Car.ax,Car.v,Vhcl.tRoad\n");

    // Write data rows
    for (size_t row = 0; row < erg.sample_count; row++) {
        for (size_t col = 0; col < num_signals; col++) {
            if (signals[col]) {
                fprintf(csv, "%.6f", signals[col][row]);
            } else {
                fprintf(csv, ""); // Empty cell if signal not available
            }

            if (col < num_signals - 1) {
                fprintf(csv, ",");
            }
        }
        fprintf(csv, "\n");
    }

    fclose(csv);

    // Calculate actual frequency from Time signal
    if (signals[0] && erg.sample_count >= 2) {
        double dt        = signals[0][1] - signals[0][0];
        double frequency = 1.0 / dt;
        printf("  Time signal frequency: %.3f Hz (dt = %.6f s)\n", frequency, dt);
        printf("  Time range: %.3f to %.3f seconds\n",
               signals[0][0], signals[0][erg.sample_count - 1]);
    }

    printf("  Wrote %zu rows to result.csv\n", erg.sample_count);
    printf("[OK] CSV export test passed\n");

    // Free all signal arrays
    for (size_t i = 0; i < num_signals; i++) {
        if (signals[i])
            free(signals[i]);
    }

    erg_free(&erg);
}

void test_erg_threading(const char* erg_path) {
    printf("\nTesting multi-threaded extraction...\n");

    /* Test both single-threaded (NULL pool) and multi-threaded (with pool) */
    ERG erg_single, erg_multi;

    /* Single-threaded */
    erg_init(&erg_single, erg_path, NULL);
    erg_parse(&erg_single);
    printf("  Single-threaded: %d thread(s) decided\n", erg_single.num_threads);

    /* Multi-threaded with 8-thread pool */
    ThreadPool* pool = thread_pool_create(8);
    erg_init(&erg_multi, erg_path, pool);
    erg_parse(&erg_multi);
    printf("  Multi-threaded:  %d thread(s) decided\n", erg_multi.num_threads);

    /* Test correctness: compare single vs multi-threaded results */
    size_t signals_tested = 0;
    size_t signals_matched = 0;

    for (size_t i = 0; i < erg_single.signal_count && i < 10; i++) {
        const char* signal_name = erg_single.signals[i].name;

        double* data_single = erg_get_signal_as_double(&erg_single, signal_name);
        double* data_multi  = erg_get_signal_as_double(&erg_multi, signal_name);

        if (!data_single || !data_multi) {
            if (data_single) free(data_single);
            if (data_multi) free(data_multi);
            continue;
        }

        signals_tested++;

        /* Compare results */
        int match = 1;
        for (size_t j = 0; j < erg_single.sample_count; j++) {
            if (fabs(data_single[j] - data_multi[j]) > EPSILON) {
                match = 0;
                printf("  [FAIL] %s - sample %zu: single=%.6f, multi=%.6f\n",
                       signal_name, j, data_single[j], data_multi[j]);
                break;
            }
        }

        if (match) {
            signals_matched++;
        }

        free(data_single);
        free(data_multi);
    }

    printf("  Tested %zu signals, %zu matched perfectly\n", signals_tested, signals_matched);
    assert(signals_tested == signals_matched);
    printf("[OK] Threading correctness test passed\n");

    erg_free(&erg_single);
    erg_free(&erg_multi);
    thread_pool_destroy(pool);
}

void test_erg_performance(const char* erg_path) {
    printf("\nPerformance Benchmark...\n");

    /* Get file info from first parse */
    ERG erg_temp;
    erg_init(&erg_temp, erg_path, NULL);
    erg_parse(&erg_temp);

    size_t signal_count = erg_temp.signal_count;
    size_t sample_count = erg_temp.sample_count;
    size_t row_size = erg_temp.row_size;
    size_t signals_to_test = signal_count < 10 ? signal_count : 10;

    printf("  File: %s\n", erg_path);
    printf("  Signals: %zu, Samples: %zu\n", signal_count, sample_count);
    printf("  Data size: %.2f MB\n", (sample_count * row_size) / (1024.0 * 1024.0));

    erg_free(&erg_temp);

    /* Create thread pool once, reuse across rounds */
    ThreadPool* pool = thread_pool_create(8);

    /* Benchmark pattern: Single(1-5), Multi(1-5), Single(1-5), Multi(1-5) */
    const int ITERATIONS = 5;
    const int ROUNDS = 2;
    double single_times[ROUNDS][ITERATIONS];
    double multi_times[ROUNDS][ITERATIONS];

    printf("\n  Benchmark (pattern: Single x5, Multi x5, Single x5, Multi x5):\n");
    printf("  Note: Each round re-initializes ERG contexts (new memory mapping)\n");

    for (int round = 0; round < ROUNDS; round++) {
        /* Initialize single-threaded context for this round */
        ERG erg_single;
        erg_init(&erg_single, erg_path, NULL);
        erg_parse(&erg_single);

        /* Run all Single iterations */
        printf("  Round %d - Single-threaded:\n", round + 1);
        for (int iter = 0; iter < ITERATIONS; iter++) {
            double start = get_time_ns();
            for (size_t i = 0; i < signals_to_test; i++) {
                double* data = erg_get_signal_as_double(&erg_single, erg_single.signals[i].name);
                if (data) {
                    free(data);
                }
            }
            double end = get_time_ns();
            single_times[round][iter] = end - start;
            printf("    Iteration %d: %.2f ms (%.0f ns)\n",
                   iter + 1, single_times[round][iter] / 1000000.0, single_times[round][iter]);
        }

        erg_free(&erg_single);

        /* Initialize multi-threaded context for this round */
        ERG erg_multi;
        erg_init(&erg_multi, erg_path, pool);
        erg_parse(&erg_multi);

        if (round == 0) {
            printf("  Multi-threaded mode decided: %d thread(s)\n", erg_multi.num_threads);
        }

        /* Run all Multi iterations */
        printf("  Round %d - Multi-threaded:\n", round + 1);
        for (int iter = 0; iter < ITERATIONS; iter++) {
            double start = get_time_ns();
            for (size_t i = 0; i < signals_to_test; i++) {
                double* data = erg_get_signal_as_double(&erg_multi, erg_multi.signals[i].name);
                if (data) {
                    free(data);
                }
            }
            double end = get_time_ns();
            multi_times[round][iter] = end - start;
            printf("    Iteration %d: %.2f ms (%.0f ns)\n",
                   iter + 1, multi_times[round][iter] / 1000000.0, multi_times[round][iter]);
        }

        erg_free(&erg_multi);
    }

    thread_pool_destroy(pool);

    /* Calculate averages across all rounds */
    double single_total_time = 0.0;
    double multi_total_time = 0.0;
    for (int round = 0; round < ROUNDS; round++) {
        for (int iter = 0; iter < ITERATIONS; iter++) {
            single_total_time += single_times[round][iter];
            multi_total_time += multi_times[round][iter];
        }
    }

    double single_avg_time = single_total_time / (ROUNDS * ITERATIONS);
    double multi_avg_time = multi_total_time / (ROUNDS * ITERATIONS);

    printf("\n  Single-threaded average: %.2f ms (%.0f ns)\n", single_avg_time / 1000000.0, single_avg_time);
    printf("  Multi-threaded average:  %.2f ms (%.0f ns)\n", multi_avg_time / 1000000.0, multi_avg_time);

    /* Calculate speedup */
    double speedup = single_avg_time / multi_avg_time;
    printf("\n  Speedup: %.2fx", speedup);
    if (speedup > 1.0) {
        printf(" (multi-threaded is faster)\n");
    } else if (speedup < 1.0) {
        printf(" (single-threaded is faster)\n");
    } else {
        printf(" (no difference)\n");
    }

    /* Calculate throughput */
    size_t bytes_per_iteration = signals_to_test * sample_count * sizeof(double);
    double single_throughput_mbps = (bytes_per_iteration / (1024.0 * 1024.0)) / (single_avg_time / 1000000000.0);
    double multi_throughput_mbps = (bytes_per_iteration / (1024.0 * 1024.0)) / (multi_avg_time / 1000000000.0);

    printf("  Single-threaded throughput: %.2f MB/s\n", single_throughput_mbps);
    printf("  Multi-threaded throughput:  %.2f MB/s\n", multi_throughput_mbps);

    printf("[OK] Performance benchmark completed\n");
}

int main(int argc, char* argv[]) {
    printf("=== ERG Parser Test ===\n\n");

    if (argc < 2) {
        fprintf(stderr, "ERROR: ERG file path required\n");
        fprintf(stderr, "Usage: %s <path/to/file.erg>\n", argv[0]);
        return 1;
    }

    const char* erg_path = argv[1];

    /* Check if file exists */
    FILE* test = fopen(erg_path, "rb");
    if (!test) {
        fprintf(stderr, "ERROR: ERG file not found: %s\n", erg_path);
        return 1;
    }
    fclose(test);

    printf("ERG file: %s\n\n", erg_path);

    test_erg_basic(erg_path);
    test_erg_signal_access(erg_path);
    test_erg_export_csv(erg_path);
    test_erg_threading(erg_path);
    test_erg_performance(erg_path);

    printf("\n=== All ERG tests passed! ===\n");
    printf("\nGenerated files:\n");
    printf("  result.csv - CSV export of Time, Car.ax, Car.v, Vhcl.tRoad\n");

    return 0;
}
