#include <infofile.h>
#include <infofile_arena.h>
#include <infofile_simd.h>
#include <infofile_arena_simd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test cases: key and expected value pairs
typedef struct {
    const char *key;
    const char *expected_value;
    const char *description;
} TestCase;

static const TestCase test_cases[] = {
    {"Link.2175.LaneSection.0.LaneR.0.ID", "477549", "Link lane ID"},
    {"FileIdent", "IPGRoad 14.0", "File identifier"},
    {"FileCreator", "CarMaker Office 14.1.1", "File creator"},
    {"Junction.0.ID", "579482", "Junction ID"},
    {"Junction.0.Type", "Area", "Junction type"},
    {"Junction.0.RST", "Countryroad", "Junction road surface type"},
    {"Route.0.Length", "1046050.30450494", "Route length"},
    {"Route.0.ID", "9495", "Route ID"},
    {"Route.0.Name", "Route_2", "Route name"},
    {"nLinks", "3535", "Number of links"},
    {"nJunctions", "2834", "Number of junctions"},
};

#define NUM_TEST_CASES (sizeof(test_cases) / sizeof(test_cases[0]))

void test_standard_parser(const char *filename)
{
    printf("Testing standard parser with %s...\n", filename);

    InfoFile info;
    infofile_init(&info);

    int result = infofile_parse_file(filename, &info);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to parse file %s\n", filename);
        exit(1);
    }

    printf("  Parsed %zu entries\n", info.count);

    // Test all test cases
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        const char *value = infofile_get(&info, test_cases[i].key);
        if (value == NULL) {
            fprintf(stderr, "ERROR: Key '%s' not found in file\n", test_cases[i].key);
            infofile_free(&info);
            exit(1);
        }

        if (strcmp(value, test_cases[i].expected_value) != 0) {
            fprintf(stderr, "ERROR: %s - Expected '%s' but got '%s'\n",
                    test_cases[i].description, test_cases[i].expected_value, value);
            infofile_free(&info);
            exit(1);
        }
        printf("  ✓ %s: %s = %s\n", test_cases[i].description, test_cases[i].key, value);
    }

    infofile_free(&info);
    printf("✓ Standard parser test passed (%zu test cases)\n", NUM_TEST_CASES);
}

void test_arena_parser(const char *filename)
{
    printf("Testing arena parser with %s...\n", filename);

    InfoFileArena info;
    infofile_arena_init(&info);

    int result = infofile_arena_parse_file(filename, &info);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to parse file %s\n", filename);
        exit(1);
    }

    printf("  Parsed %zu entries\n", info.count);

    // Test all test cases
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        const char *value = infofile_arena_get(&info, test_cases[i].key);
        if (value == NULL) {
            fprintf(stderr, "ERROR: Key '%s' not found in file\n", test_cases[i].key);
            infofile_arena_free(&info);
            exit(1);
        }

        if (strcmp(value, test_cases[i].expected_value) != 0) {
            fprintf(stderr, "ERROR: %s - Expected '%s' but got '%s'\n",
                    test_cases[i].description, test_cases[i].expected_value, value);
            infofile_arena_free(&info);
            exit(1);
        }
        printf("  ✓ %s: %s = %s\n", test_cases[i].description, test_cases[i].key, value);
    }

    infofile_arena_free(&info);
    printf("✓ Arena parser test passed (%zu test cases)\n", NUM_TEST_CASES);
}

void test_simd_parser(const char *filename)
{
    printf("Testing SIMD parser with %s...\n", filename);

    InfoFileSIMD info;
    infofile_simd_init(&info);

    int result = infofile_simd_parse_file(filename, &info);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to parse file %s\n", filename);
        exit(1);
    }

    printf("  Parsed %zu entries\n", info.count);

    // Test all test cases
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        const char *value = infofile_simd_get(&info, test_cases[i].key);
        if (value == NULL) {
            fprintf(stderr, "ERROR: Key '%s' not found in file\n", test_cases[i].key);
            infofile_simd_free(&info);
            exit(1);
        }

        if (strcmp(value, test_cases[i].expected_value) != 0) {
            fprintf(stderr, "ERROR: %s - Expected '%s' but got '%s'\n",
                    test_cases[i].description, test_cases[i].expected_value, value);
            infofile_simd_free(&info);
            exit(1);
        }
        printf("  ✓ %s: %s = %s\n", test_cases[i].description, test_cases[i].key, value);
    }

    infofile_simd_free(&info);
    printf("✓ SIMD parser test passed (%zu test cases)\n", NUM_TEST_CASES);
}

void test_arena_simd_parser(const char *filename)
{
    printf("Testing Arena+SIMD parser with %s...\n", filename);

    InfoFileArenaSimd info;
    infofile_arena_simd_init(&info);

    int result = infofile_arena_simd_parse_file(filename, &info);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to parse file %s\n", filename);
        exit(1);
    }

    printf("  Parsed %zu entries\n", info.count);

    // Test all test cases
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        const char *value = infofile_arena_simd_get(&info, test_cases[i].key);
        if (value == NULL) {
            fprintf(stderr, "ERROR: Key '%s' not found in file\n", test_cases[i].key);
            infofile_arena_simd_free(&info);
            exit(1);
        }

        if (strcmp(value, test_cases[i].expected_value) != 0) {
            fprintf(stderr, "ERROR: %s - Expected '%s' but got '%s'\n",
                    test_cases[i].description, test_cases[i].expected_value, value);
            infofile_arena_simd_free(&info);
            exit(1);
        }
        printf("  ✓ %s: %s = %s\n", test_cases[i].description, test_cases[i].key, value);
    }

    infofile_arena_simd_free(&info);
    printf("✓ Arena+SIMD parser test passed (%zu test cases)\n", NUM_TEST_CASES);
}

int main(int argc, char *argv[])
{
    const char *filename;

    if (argc > 1) {
        filename = argv[1];
    } else {
        // Try to find the file - it might be run from build/ or from project root
        FILE *test = fopen("example/road.rd5", "rb");
        if (test) {
            fclose(test);
            filename = "example/road.rd5";
        } else {
            test = fopen("../example/road.rd5", "rb");
            if (test) {
                fclose(test);
                filename = "../example/road.rd5";
            } else {
                fprintf(stderr, "ERROR: Cannot find example/road.rd5\n");
                fprintf(stderr, "Please run from project root or pass file path as argument\n");
                return 1;
            }
        }
    }

    printf("=== InfoFile Correctness Test ===\n");
    printf("File: %s\n", filename);
    printf("Testing %zu key-value pairs\n\n", NUM_TEST_CASES);

    test_standard_parser(filename);
    printf("\n");
    test_arena_parser(filename);
    printf("\n");
    test_simd_parser(filename);
    printf("\n");
    test_arena_simd_parser(filename);

    printf("\n=== All correctness tests passed! ===\n");
    printf("All 4 parsers correctly parsed %zu test entries\n", NUM_TEST_CASES);
    return 0;
}
