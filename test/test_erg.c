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

    printf("File: %s\n", erg_path);
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

/* 4. Test signal extraction */
void test_signal_extraction(const char* erg_path) {
    printf("\n=== Test 4: Signal Extraction ===\n");

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    /* Test multiple signals */
    const char* test_signals[] = {"Time", "Car.ax", "Car.v"};
    size_t num_signals = 3;

    printf("Extracting %zu signals...\n", num_signals);

    for (size_t i = 0; i < num_signals; i++) {
        const ERGSignal* sig_info = erg_get_signal_info(&erg, test_signals[i]);
        void* data = erg_get_signal(&erg, test_signals[i]);

        if (data && sig_info) {
            printf("  %s (type=%d): ", test_signals[i], sig_info->type);

            /* Print first and last value based on actual type */
            switch (sig_info->type) {
            case ERG_FLOAT:
                printf("first=%.6f, last=%.6f\n",
                       ((float*)data)[0], ((float*)data)[erg.sample_count - 1]);
                break;
            case ERG_DOUBLE:
                printf("first=%.6f, last=%.6f\n",
                       ((double*)data)[0], ((double*)data)[erg.sample_count - 1]);
                break;
            case ERG_INT:
                printf("first=%d, last=%d\n",
                       ((int32_t*)data)[0], ((int32_t*)data)[erg.sample_count - 1]);
                break;
            case ERG_UINT:
                printf("first=%u, last=%u\n",
                       ((uint32_t*)data)[0], ((uint32_t*)data)[erg.sample_count - 1]);
                break;
            default:
                printf("(unsupported type for display)\n");
                break;
            }
            free(data);
        } else {
            printf("  %s: not found\n", test_signals[i]);
        }
    }

    printf("[OK] Signal extraction completed\n");
    erg_free(&erg);
}

/* 5. Export CSV test */
void test_export_csv(const char* erg_path) {
    printf("\n=== Test 5: Export CSV ===\n");

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

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

    /* Extract all signals with their type information */
    void* signals[6] = {NULL};
    const ERGSignal* signal_info[6] = {NULL};

    for (size_t i = 0; i < num_signals; i++) {
        signal_info[i] = erg_get_signal_info(&erg, signal_names[i]);
        signals[i] = erg_get_signal(&erg, signal_names[i]);
    }

    /* Create filename */
    const char* filename = "result.csv";

    /* Write CSV */
    FILE* csv = fopen(filename, "w");
    if (!csv) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
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

    printf("Exported %s (%zu rows)\n", filename, erg.sample_count);

    /* Free all signal arrays */
    for (size_t i = 0; i < num_signals; i++) {
        if (signals[i])
            free(signals[i]);
    }

    printf("[OK] Export completed\n");
    erg_free(&erg);
}

/* 6. Simple performance benchmark */
void test_benchmark(const char* erg_path) {
    printf("\n=== Test 6: Performance Benchmark ===\n");

    const char* signal_name = "Time";
    const int iterations = 10;

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    printf("Signal: %s\n", signal_name);
    printf("Iterations: %d\n\n", iterations);

    double total_time = 0.0;
    double min_time = 1e9;

    for (int iter = 0; iter < iterations; iter++) {
        double start_time = get_time_seconds();
        double* data = (double*)erg_get_signal(&erg, signal_name);
        double end_time = get_time_seconds();

        double elapsed = (end_time - start_time) * 1000.0;
        total_time += elapsed;
        if (elapsed < min_time) {
            min_time = elapsed;
        }

        free(data);
    }

    double avg_time = total_time / iterations;
    printf("Average time: %.3f ms\n", avg_time);
    printf("Minimum time: %.3f ms\n", min_time);

    printf("[OK] Benchmark completed\n");
    erg_free(&erg);
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
    test_signal_extraction(erg_path);
    test_export_csv(erg_path);
    test_benchmark(erg_path);

    printf("\n=== All Tests Passed! ===\n");
    printf("\nGenerated result.csv file for validation.\n");

    return 0;
}
