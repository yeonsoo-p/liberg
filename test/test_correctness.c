#include <infofile.h>
#include <infofile_arena.h>
#include <infofile_simd.h>
#include <infofile_arena_simd.h>
#include <infofile_arena_simd_opt.h>
#include <infofile_simple.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test cases: key and expected value pairs
// Sampled from different locations in the 996K line file
typedef struct {
    const char *key;
    const char *expected_value;
    const char *description;
} TestCase;

static const TestCase test_cases[] = {
    // Beginning of file
    {"FileIdent", "IPGRoad 14.0", "File identifier (line 2)"},
    {"FileCreator", "CarMaker Office 14.1.1", "File creator (line 3)"},

    // Early section
    {"Junction.0.ID", "579482", "Junction ID"},
    {"Junction.0.Type", "Area", "Junction type"},
    {"Junction.0.RST", "Countryroad", "Junction road surface type"},
    {"Route.0.Length", "1046050.30450494", "Route length"},
    {"Route.0.ID", "9495", "Route ID"},
    {"Route.0.Name", "Route_2", "Route name"},
    {"nLinks", "3535", "Number of links"},
    {"nJunctions", "2834", "Number of junctions"},

    // Middle section
    {"Link.2175.LaneSection.0.LaneR.0.ID", "477549", "Link lane ID (middle)"},

    // Late section (around line 900,000)
    {"Link.3485.LateralCenterLineOffset.ID", "894619", "Link 3485 offset ID (late)"},
    {"Link.3485.LaneSection.0.ID", "894558", "Link 3485 lane section (late)"},
    {"Link.3485.LaneSection.0.Start", "0", "Link 3485 section start (late)"},

    // End of file
    {"Control.TrfLight.68", "941160 JuncArm_381952 Time>=0.000000 3 0 15 4 28 4", "Traffic light 68 (end)"},
    {"Control.TrfLight.69", "941161 CtrlTL015 \"\" 1 0 15 3 15 3", "Traffic light 69 (end)"},
    {"MaxUsedObjId", "941652", "Max object ID (line 996366)"},

    // Multiline values (key: followed by indented lines)
    {"Junction.0.Link.0.LaneSection.0.LaneL.0.Width", "445061 -1 0 0 1 3.99495155267296 0 -999 -999\n362229 -1 0 1 1 3.78377990903144 0 -999 -999", "Multiline width (2 lines)"},
    {"Junction.0.Link.0.LaneSection.0.LaneR.0.Width", "445062 -1 0 0 1 4.00700290085828 0 -999 -999\n362227 -1 0 1 1 3.78039994814133 0 -999 -999", "Multiline width (2 lines)"},
    {"Junction.1.HMMesh.DeltaU", "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1", "Multiline DeltaU (1 line)"},
    {"Junction.3.RL.3.Seg.0.Curve.Points", "362894 362565 -0.820901971666899 45.6687762230722 0 0 0 100 277.903092940284 -999\n362895 362565 3.2474857365014 16.4614153836155 0 0 0 100 -999 -999\n362896 362565 4.77912495504279 5.3511128462851 0 0 0 100 277.720834107522 -999", "Multiline curve points (3 lines)"},
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

void test_arena_simd_opt_parser(const char *filename)
{
    printf("Testing Arena+SIMD Optimized parser with %s...\n", filename);

    InfoFileArenaSimdOpt info;
    infofile_arena_simd_opt_init(&info);

    int result = infofile_arena_simd_opt_parse_file(filename, &info);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to parse file %s\n", filename);
        exit(1);
    }

    printf("  Parsed %zu entries\n", info.count);

    // Test all test cases
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        const char *value = infofile_arena_simd_opt_get(&info, test_cases[i].key);
        if (value == NULL) {
            fprintf(stderr, "ERROR: Key '%s' not found in file\n", test_cases[i].key);
            infofile_arena_simd_opt_free(&info);
            exit(1);
        }

        if (strcmp(value, test_cases[i].expected_value) != 0) {
            fprintf(stderr, "ERROR: %s - Expected '%s' but got '%s'\n",
                    test_cases[i].description, test_cases[i].expected_value, value);
            infofile_arena_simd_opt_free(&info);
            exit(1);
        }
        printf("  ✓ %s: %s = %s\n", test_cases[i].description, test_cases[i].key, value);
    }

    infofile_arena_simd_opt_free(&info);
    printf("✓ Arena+SIMD Optimized parser test passed (%zu test cases)\n", NUM_TEST_CASES);
}

void test_simple_parser(const char *filename)
{
    printf("Testing Simple parser with %s...\n", filename);

    InfoFileSimple info;
    infofile_simple_init(&info);

    int result = infofile_simple_parse_file(filename, &info);
    if (result != 0) {
        fprintf(stderr, "ERROR: Failed to parse file %s\n", filename);
        exit(1);
    }

    printf("  Parsed %zu entries\n", info.count);

    // Test all test cases
    for (size_t i = 0; i < NUM_TEST_CASES; i++) {
        const char *value = infofile_simple_get(&info, test_cases[i].key);
        if (value == NULL) {
            fprintf(stderr, "ERROR: Key '%s' not found in file\n", test_cases[i].key);
            infofile_simple_free(&info);
            exit(1);
        }

        if (strcmp(value, test_cases[i].expected_value) != 0) {
            fprintf(stderr, "ERROR: %s - Expected '%s' but got '%s'\n",
                    test_cases[i].description, test_cases[i].expected_value, value);
            infofile_simple_free(&info);
            exit(1);
        }
        printf("  ✓ %s: %s = %s\n", test_cases[i].description, test_cases[i].key, value);
    }

    infofile_simple_free(&info);
    printf("✓ Simple parser test passed (%zu test cases)\n", NUM_TEST_CASES);
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
    printf("\n");
    test_arena_simd_opt_parser(filename);
    printf("\n");
    test_simple_parser(filename);

    printf("\n=== All correctness tests passed! ===\n");
    printf("All 6 parsers correctly parsed %zu test entries\n", NUM_TEST_CASES);
    return 0;
}
