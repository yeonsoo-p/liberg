#include <erginfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <erg.info file>\n", argv[0]);
        return 1;
    }

    ErgInfo info;
    erginfo_init(&info);

    printf("Parsing %s...\n", argv[1]);
    if (erginfo_parse_file(argv[1], &info) != 0) {
        fprintf(stderr, "Error: Could not parse file %s\n", argv[1]);
        erginfo_free(&info);
        return 1;
    }

    printf("Successfully parsed %zu entries\n\n", info.count);

    // Display some key values
    const char *format = erginfo_get(&info, "File.Format");
    if (format) {
        printf("File Format: %s\n", format);
    }

    const char *byte_order = erginfo_get(&info, "File.ByteOrder");
    if (byte_order) {
        printf("Byte Order: %s\n", byte_order);
    }

    const char *testrun = erginfo_get(&info, "Testrun");
    if (testrun) {
        printf("Testrun: %s\n", testrun);
    }

    const char *version = erginfo_get(&info, "CarMaker.Version");
    if (version) {
        printf("CarMaker Version: %s\n", version);
    }

    printf("\nFirst 10 entries:\n");
    for (size_t i = 0; i < info.count && i < 10; i++) {
        const char *value = info.entries[i].value;
        // Truncate long values
        if (strlen(value) > 60) {
            printf("  %s = %.60s...\n", info.entries[i].key, value);
        } else {
            printf("  %s = %s\n", info.entries[i].key, value);
        }
    }

    erginfo_free(&info);
    return 0;
}
