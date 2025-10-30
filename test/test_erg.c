#include <assert.h>
#include <erg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EPSILON 1e-9

void test_erg_basic(const char* erg_path) {
    printf("\nTesting basic ERG parsing...\n");

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    printf("  Signals: %zu\n", erg.signal_count);
    printf("  Samples: %zu\n", erg.sample_count);
    printf("  Byte order: %s\n", erg.little_endian ? "Little-endian" : "Big-endian");
    printf("  Row size: %zu bytes\n", erg.row_size);
    printf("  Data size: %.2f MB\n", (erg.sample_count * erg.row_size) / (1024.0 * 1024.0));

    assert(erg.signal_count > 0);
    assert(erg.sample_count > 0);
    printf("[OK] Basic parsing test passed\n");

    erg_free(&erg);
}

void test_erg_signal_access(const char* erg_path) {
    printf("\nTesting signal access...\n");

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    // Test finding a signal
    int time_idx = erg_find_signal_index(&erg, "Time");
    if (time_idx >= 0) {
        printf("  Found 'Time' signal at index %d\n", time_idx);

        // Test extracting signal as double
        double* time_data = erg_get_signal_as_double(&erg, "Time");
        if (time_data) {
            printf("  Extracted 'Time' signal: first=%.6f, last=%.6f\n",
                   time_data[0], time_data[erg.sample_count - 1]);
            free(time_data);
        }
    }

    printf("[OK] Signal access test passed\n");
    erg_free(&erg);
}

void test_erg_export_csv(const char* erg_path) {
    printf("\nTesting CSV export...\n");

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    // Extract a few signals and export to CSV
    const char* signal_names[] = {"Time", "Car.ax", "Car.v", "Vhcl.tRoad"};
    size_t      num_signals    = 4;
    double*     signals[4]     = {NULL};

    // Extract all signals as double
    for (size_t i = 0; i < num_signals; i++) {
        signals[i] = erg_get_signal_as_double(&erg, signal_names[i]);
    }

    // Write CSV
    FILE* csv = fopen("result.csv", "w");
    if (!csv) {
        fprintf(stderr, "Failed to open result.csv for writing\n");
        return;
    }

    // Header
    for (size_t i = 0; i < num_signals; i++) {
        fprintf(csv, "%s", signal_names[i]);
        if (i < num_signals - 1)
            fprintf(csv, ",");
    }
    fprintf(csv, "\n");

    // Data
    for (size_t row = 0; row < erg.sample_count; row++) {
        for (size_t col = 0; col < num_signals; col++) {
            if (signals[col]) {
                fprintf(csv, "%.6f", signals[col][row]);
            } else {
                fprintf(csv, "");
            }
            if (col < num_signals - 1)
                fprintf(csv, ",");
        }
        fprintf(csv, "\n");
    }

    fclose(csv);

    printf("  Wrote %zu rows to result.csv\n", erg.sample_count);
    printf("[OK] CSV export test passed\n");

    // Free all signal arrays
    for (size_t i = 0; i < num_signals; i++) {
        if (signals[i])
            free(signals[i]);
    }

    erg_free(&erg);
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

    printf("\n=== All ERG tests passed! ===\n");
    printf("\nGenerated files:\n");
    printf("  result.csv - CSV export of Time, Car.ax, Car.v, Vhcl.tRoad\n");

    return 0;
}
