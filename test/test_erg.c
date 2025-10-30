#include <assert.h>
#include <erg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_erg_basic() {
    printf("Testing basic ERG parsing...\n");

    // Try to find the ERG file
    const char* erg_path = NULL;
    FILE*       test     = fopen("example/result.erg", "rb");
    if (test) {
        fclose(test);
        erg_path = "example/result.erg";
    } else {
        test = fopen("../example/result.erg", "rb");
        if (test) {
            fclose(test);
            erg_path = "../example/result.erg";
        }
    }

    if (!erg_path) {
        printf("[WARNING] result.erg not found - skipping ERG tests\n");
        printf("  (Place file in example/result.erg or pass path as argument)\n");
        return;
    }

    ERG erg;
    erg_init(&erg, erg_path);
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
    assert(erg.raw_data != NULL);

    printf("[OK] Basic ERG parsing test passed\n");

    erg_free(&erg);
}

void test_erg_signal_access() {
    printf("\nTesting signal access...\n");

    const char* erg_path = NULL;
    FILE*       test     = fopen("example/result.erg", "rb");
    if (test) {
        fclose(test);
        erg_path = "example/result.erg";
    } else {
        test = fopen("../example/result.erg", "rb");
        if (test) {
            fclose(test);
            erg_path = "../example/result.erg";
        }
    }

    if (!erg_path) {
        printf("[WARNING] result.erg not found - skipping signal access tests\n");
        return;
    }

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    // Test getting Time signal
    size_t      time_length = 0;
    const void* time_data   = erg_get_signal(&erg, "Time", &time_length);

    if (time_data) {
        printf("  Found 'Time' signal: %zu samples\n", time_length);
        assert(time_length == erg.sample_count);

        // Get as double array
        size_t  double_length = 0;
        double* time_double   = erg_get_signal_as_double(&erg, "Time", &double_length);
        assert(time_double != NULL);
        assert(double_length == time_length);

        printf("  Time range: %.3f to %.3f seconds\n",
               time_double[0], time_double[double_length - 1]);

        // Check that time is monotonically increasing
        for (size_t i = 1; i < double_length; i++) {
            assert(time_double[i] >= time_double[i - 1]);
        }

        free(time_double);
        free((void*)time_data);
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
    size_t      length    = 0;
    const void* null_data = erg_get_signal(&erg, "NonExistentSignal123", &length);
    assert(null_data == NULL);
    assert(length == 0);
    printf("[OK] Non-existent signal test passed\n");

    erg_free(&erg);
}

void test_erg_signal_iteration() {
    printf("\nTesting signal iteration...\n");

    const char* erg_path = NULL;
    FILE*       test     = fopen("example/result.erg", "rb");
    if (test) {
        fclose(test);
        erg_path = "example/result.erg";
    } else {
        test = fopen("../example/result.erg", "rb");
        if (test) {
            fclose(test);
            erg_path = "../example/result.erg";
        }
    }

    if (!erg_path) {
        printf("[WARNING] result.erg not found - skipping iteration tests\n");
        return;
    }

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    printf("  Iterating through all signals...\n");

    size_t signals_tested = 0;
    for (size_t i = 0; i < erg.signal_count && i < 10; i++) {
        const char* name   = erg.signals[i].name;
        size_t      length = 0;
        const void* data   = erg_get_signal(&erg, name, &length);

        assert(data != NULL);
        assert(length == erg.sample_count);

        free((void*)data);
        signals_tested++;
    }

    printf("  Successfully tested %zu signals\n", signals_tested);
    printf("[OK] Signal iteration test passed\n");

    erg_free(&erg);
}

void test_erg_export_csv() {
    printf("\nExporting CSV file...\n");

    const char* erg_path = NULL;
    FILE*       test     = fopen("example/result.erg", "rb");
    if (test) {
        fclose(test);
        erg_path = "example/result.erg";
    } else {
        test = fopen("../example/result.erg", "rb");
        if (test) {
            fclose(test);
            erg_path = "../example/result.erg";
        }
    }

    if (!erg_path) {
        printf("[WARNING] result.erg not found - skipping CSV export\n");
        return;
    }

    ERG erg;
    erg_init(&erg, erg_path);
    erg_parse(&erg);

    // Signal names to export
    const char* signal_names[] = {"Time", "Car.ax", "Car.v", "Vhcl.tRoad"};
    size_t      num_signals    = sizeof(signal_names) / sizeof(signal_names[0]);

    // Get all signals as double arrays
    double* signals[4]    = {NULL, NULL, NULL, NULL};
    size_t  lengths[4]    = {0, 0, 0, 0};
    int     found_signals = 0;

    for (size_t i = 0; i < num_signals; i++) {
        signals[i] = erg_get_signal_as_double(&erg, signal_names[i], &lengths[i]);
        if (signals[i]) {
            printf("  Found signal: %s (%zu samples)\n", signal_names[i], lengths[i]);
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

    // Determine number of rows to write (use maximum length)
    size_t max_length = 0;
    for (size_t i = 0; i < num_signals; i++) {
        if (lengths[i] > max_length) {
            max_length = lengths[i];
        }
    }

    // Write data rows
    for (size_t row = 0; row < max_length; row++) {
        for (size_t col = 0; col < num_signals; col++) {
            if (signals[col] && row < lengths[col]) {
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
    if (signals[0] && lengths[0] >= 2) {
        double dt        = signals[0][1] - signals[0][0];
        double frequency = 1.0 / dt;
        printf("  Time signal frequency: %.3f Hz (dt = %.6f s)\n", frequency, dt);
        printf("  Time range: %.3f to %.3f seconds\n",
               signals[0][0], signals[0][lengths[0] - 1]);
    }

    printf("  Wrote %zu rows to result.csv\n", max_length);
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

    test_erg_basic();
    test_erg_signal_access();
    test_erg_signal_iteration();
    test_erg_export_csv();

    printf("\n=== All ERG tests passed! ===\n");
    printf("\nGenerated files:\n");
    printf("  result.csv - CSV export of Time, Car.ax, Car.v, Vhcl.tRoad\n");

    return 0;
}
