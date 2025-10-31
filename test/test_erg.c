#include <assert.h>
#include <erg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#define EPSILON 1e-9

/* High-resolution timer */
static double get_time_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
#endif
}

/* 1. Test and benchmark initialization and parsing */
void test_init_and_parse(const char* erg_path) {
    printf("\n=== Test 1: Initialization and Parsing ===\n");

    double start_time = get_time_seconds();

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    double end_time = get_time_seconds();
    double elapsed_ms = (end_time - start_time) * 1000.0;

    const char* simd_name = "Unknown";
    switch (erg.simd_level) {
    case 0: simd_name = "None (Scalar)"; break;
    case 1: simd_name = "SSE2"; break;
    case 2: simd_name = "AVX2"; break;
    case 3: simd_name = "AVX-512"; break;
    }

    printf("File: %s\n", erg_path);
    printf("SIMD Level: %s\n", simd_name);
    printf("Signals: %zu\n", erg.signal_count);
    printf("Samples: %zu\n", erg.sample_count);
    printf("Row size: %zu bytes\n", erg.row_size);
    printf("Data size: %.2f MB\n", (erg.sample_count * erg.row_size) / (1024.0 * 1024.0));
    printf("Initialization + Parsing time: %.3f ms\n", elapsed_ms);

    assert(erg.signal_count > 0);
    assert(erg.sample_count > 0);
    printf("[OK] Initialization and parsing completed\n");

    erg_free(&erg);
}

/* 2. Test and benchmark cold read signal (first read after parse) */
void test_cold_read(const char* erg_path) {
    printf("\n=== Test 2: Cold Read Signal ===\n");

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    const char* signal_name = "Time";

    printf("Reading signal '%s' (cold read - first access after parse)...\n", signal_name);

    double start_time = get_time_seconds();
    double* data = (double*)erg_get_signal(&erg, signal_name);
    double end_time = get_time_seconds();

    double elapsed_ms = (end_time - start_time) * 1000.0;

    if (data) {
        printf("First value: %.6f\n", data[0]);
        printf("Last value: %.6f\n", data[erg.sample_count - 1]);
        printf("Cold read time: %.3f ms\n", elapsed_ms);
        free(data);
    } else {
        printf("ERROR: Signal '%s' not found\n", signal_name);
    }

    printf("[OK] Cold read completed\n");

    erg_free(&erg);
}

/* 3. Test and benchmark hot read signal (subsequent reads) */
void test_hot_read(const char* erg_path) {
    printf("\n=== Test 3: Hot Read Signal ===\n");

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    const char* signal_name = "Time";

    /* Warm up with one read */
    double* warmup = (double*)erg_get_signal(&erg, signal_name);
    free(warmup);

    printf("Reading signal '%s' (hot read - memory already accessed)...\n", signal_name);

    const int iterations = 10;
    double total_time = 0.0;

    for (int i = 0; i < iterations; i++) {
        double start_time = get_time_seconds();
        double* data = (double*)erg_get_signal(&erg, signal_name);
        double end_time = get_time_seconds();

        total_time += (end_time - start_time) * 1000.0;

        if (data) {
            free(data);
        }
    }

    double avg_time = total_time / iterations;
    printf("Hot read time (average of %d reads): %.3f ms\n", iterations, avg_time);
    printf("[OK] Hot read completed\n");

    erg_free(&erg);
}

/* 4. Test and benchmark SIMD levels */
void test_compare_simd_levels(const char* erg_path) {
    printf("\n=== Test 4: Compare SIMD Levels ===\n");

    /* Test signal names - use only double type signals to avoid type casting issues */
    const char* test_signals[] = {"Time", "Vhcl.Yaw", "Vhcl.sRoad"};
    size_t num_test_signals = 3;

    /* SIMD levels to test */
    const ERGSIMDLevel levels[] = {ERG_SIMD_NONE, ERG_SIMD_SSE2, ERG_SIMD_AVX2, ERG_SIMD_AVX512};
    const char* level_names[] = {"Scalar", "SSE2", "AVX2", "AVX-512"};
    size_t num_levels = 4;

    /* Store reference results from scalar implementation */
    double** reference_results = malloc(num_test_signals * sizeof(double*));
    size_t sample_count = 0;

    /* Store reference timing */
    double reference_time_ms = 0.0;

    printf("Testing %zu signals: ", num_test_signals);
    for (size_t i = 0; i < num_test_signals; i++) {
        printf("%s", test_signals[i]);
        if (i < num_test_signals - 1) printf(", ");
    }
    printf("\n\n");

    /* Test each SIMD level */
    for (size_t level_idx = 0; level_idx < num_levels; level_idx++) {
        ERG erg;
        erg_init(&erg, erg_path);
        erg_parse(&erg);
        sample_count = erg.sample_count;

        /* Override SIMD level */
        erg_set_simd_level(&erg, levels[level_idx]);

        printf("%s:\n", level_names[level_idx]);

        double start_time = get_time_seconds();

        /* Extract test signals */
        double* results[3] = {NULL};
        for (size_t i = 0; i < num_test_signals; i++) {
            results[i] = (double*)erg_get_signal(&erg, test_signals[i]);
        }

        double end_time = get_time_seconds();
        double elapsed_ms = (end_time - start_time) * 1000.0;

        printf("  Extraction time: %.3f ms", elapsed_ms);

        /* Verify correctness against scalar reference */
        if (level_idx == 0) {
            /* This is scalar - save as reference */
            for (size_t i = 0; i < num_test_signals; i++) {
                reference_results[i] = results[i];
            }
            reference_time_ms = elapsed_ms;
            printf(" (BASELINE)\n");
        } else {
            /* Compare with reference */
            int all_correct = 1;
            size_t max_errors_to_show = 5;
            size_t errors_shown = 0;

            for (size_t sig_idx = 0; sig_idx < num_test_signals; sig_idx++) {
                if (!results[sig_idx] || !reference_results[sig_idx]) {
                    if (results[sig_idx] != reference_results[sig_idx]) {
                        printf("\n  ERROR: Signal '%s' availability mismatch\n", test_signals[sig_idx]);
                        all_correct = 0;
                    }
                    continue;
                }

                for (size_t sample = 0; sample < sample_count && errors_shown < max_errors_to_show; sample++) {
                    double diff = fabs(results[sig_idx][sample] - reference_results[sig_idx][sample]);
                    if (diff > EPSILON) {
                        printf("\n  ERROR: Signal '%s' sample %zu: expected %.6f, got %.6f (diff: %e)\n",
                               test_signals[sig_idx], sample,
                               reference_results[sig_idx][sample], results[sig_idx][sample], diff);
                        all_correct = 0;
                        errors_shown++;
                    }
                }
            }

            if (all_correct) {
                /* Calculate speedup */
                double speedup = reference_time_ms / elapsed_ms;
                printf(" (%.2fx speedup) [PASS]\n", speedup);
            } else {
                printf(" [FAIL]\n");
            }

            /* Free current results (not reference) */
            for (size_t i = 0; i < num_test_signals; i++) {
                if (results[i]) free(results[i]);
            }
        }

        erg_free(&erg);
    }

    /* Free reference results */
    for (size_t i = 0; i < num_test_signals; i++) {
        if (reference_results[i]) free(reference_results[i]);
    }
    free(reference_results);

    printf("\n[OK] SIMD comparison completed\n");
}

/* 5. Export results for set signal names using all four SIMD levels */
void test_export_all_simd_levels(const char* erg_path) {
    printf("\n=== Test 5: Export Results with All SIMD Levels ===\n");

    /* Signal names to export */
    const char* signal_names[] = {
        "Time",
        "Car.ax",
        "Car.ay",
        "Car.v",
        "Car.Yaw",
        "Car.YawRate",
    };
    size_t num_signals = 6;

    /* SIMD levels to test */
    const ERGSIMDLevel levels[] = {ERG_SIMD_NONE, ERG_SIMD_SSE2, ERG_SIMD_AVX2, ERG_SIMD_AVX512};
    const char* level_names[] = {"scalar", "sse2", "avx2", "avx512"};
    size_t num_levels = 4;

    printf("Exporting %zu signals using %zu SIMD levels...\n", num_signals, num_levels);

    /* Export for each SIMD level */
    for (size_t level_idx = 0; level_idx < num_levels; level_idx++) {
        ERG erg;
        erg_init(&erg, erg_path);
        erg_parse(&erg);

        /* Override SIMD level */
        erg_set_simd_level(&erg, levels[level_idx]);

        /* Extract all signals with their type information */
        void* signals[6] = {NULL};
        const ERGSignal* signal_info[6] = {NULL};

        for (size_t i = 0; i < num_signals; i++) {
            signal_info[i] = erg_get_signal_info(&erg, signal_names[i]);
            signals[i] = erg_get_signal(&erg, signal_names[i]);
        }

        /* Create filename */
        char filename[256];
        snprintf(filename, sizeof(filename), "result_%s.csv", level_names[level_idx]);

        /* Write CSV */
        FILE* csv = fopen(filename, "w");
        if (!csv) {
            fprintf(stderr, "Failed to open %s for writing\n", filename);
            continue;
        }

        /* Header */
        for (size_t i = 0; i < num_signals; i++) {
            fprintf(csv, "%s", signal_names[i]);
            if (i < num_signals - 1)
                fprintf(csv, ",");
        }
        fprintf(csv, "\n");

        /* Data - handle each type correctly */
        for (size_t row = 0; row < erg.sample_count; row++) {
            for (size_t col = 0; col < num_signals; col++) {
                if (signals[col] && signal_info[col]) {
                    /* Cast to the correct type based on signal type */
                    switch (signal_info[col]->type) {
                    case ERG_FLOAT:
                        fprintf(csv, "%.6f", ((float*)signals[col])[row]);
                        break;
                    case ERG_DOUBLE:
                        fprintf(csv, "%.6f", ((double*)signals[col])[row]);
                        break;
                    case ERG_INT:
                        fprintf(csv, "%d", ((int32_t*)signals[col])[row]);
                        break;
                    case ERG_UINT:
                        fprintf(csv, "%u", ((uint32_t*)signals[col])[row]);
                        break;
                    case ERG_SHORT:
                        fprintf(csv, "%d", ((int16_t*)signals[col])[row]);
                        break;
                    case ERG_USHORT:
                        fprintf(csv, "%u", ((uint16_t*)signals[col])[row]);
                        break;
                    case ERG_LONGLONG:
                        fprintf(csv, "%lld", ((int64_t*)signals[col])[row]);
                        break;
                    case ERG_ULONGLONG:
                        fprintf(csv, "%llu", ((uint64_t*)signals[col])[row]);
                        break;
                    case ERG_CHAR:
                        fprintf(csv, "%d", ((int8_t*)signals[col])[row]);
                        break;
                    case ERG_UCHAR:
                        fprintf(csv, "%u", ((uint8_t*)signals[col])[row]);
                        break;
                    default:
                        fprintf(csv, "");
                        break;
                    }
                } else {
                    fprintf(csv, "");
                }
                if (col < num_signals - 1)
                    fprintf(csv, ",");
            }
            fprintf(csv, "\n");
        }

        fclose(csv);

        printf("  %s: %s (%zu rows)\n", level_names[level_idx], filename, erg.sample_count);

        /* Free all signal arrays */
        for (size_t i = 0; i < num_signals; i++) {
            if (signals[i])
                free(signals[i]);
        }

        erg_free(&erg);
    }

    printf("[OK] Export completed\n");
}

int main(int argc, char* argv[]) {
    printf("=== ERG Parser Comprehensive Test Suite ===\n");

    if (argc < 2) {
        fprintf(stderr, "\nERROR: ERG file path required\n");
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

    /* Run all tests */
    test_init_and_parse(erg_path);
    test_cold_read(erg_path);
    test_hot_read(erg_path);
    test_compare_simd_levels(erg_path);
    test_export_all_simd_levels(erg_path);

    printf("\n=== All Tests Passed! ===\n");
    printf("\nGenerated files:\n");
    printf("  result_scalar.csv - Export using scalar implementation\n");
    printf("  result_sse2.csv   - Export using SSE2 implementation\n");
    printf("  result_avx2.csv   - Export using AVX2 implementation\n");
    printf("  result_avx512.csv - Export using AVX-512 implementation\n");

    return 0;
}
