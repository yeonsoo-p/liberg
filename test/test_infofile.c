#include <assert.h>
#include <infofile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test cases for road.rd5 (large file with ~385K entries)
typedef struct {
    const char* key;
    const char* expected_value;
    const char* description;
} TestCase;

static const TestCase road_test_cases[] = {
    // Beginning of file
    {                                    "FileIdent",                                                                                                                                                                                                                          "IPGRoad 14.0",         "File identifier (line 2)"},
    {                                  "FileCreator",                                                                                                                                                                                                                "CarMaker Office 14.1.1",            "File creator (line 3)"},

    // Early section
    {                                "Junction.0.ID",                                                                                                                                                                                                                                "579482",                      "Junction ID"},
    {                              "Junction.0.Type",                                                                                                                                                                                                                                  "Area",                    "Junction type"},
    {                               "Junction.0.RST",                                                                                                                                                                                                                           "Countryroad",       "Junction road surface type"},
    {                               "Route.0.Length",                                                                                                                                                                                                                      "1046050.30450494",                     "Route length"},
    {                                   "Route.0.ID",                                                                                                                                                                                                                                  "9495",                         "Route ID"},
    {                                 "Route.0.Name",                                                                                                                                                                                                                               "Route_2",                       "Route name"},
    {                                       "nLinks",                                                                                                                                                                                                                                  "3535",                  "Number of links"},
    {                                   "nJunctions",                                                                                                                                                                                                                                  "2834",              "Number of junctions"},

    // Middle section
    {           "Link.2175.LaneSection.0.LaneR.0.ID",                                                                                                                                                                                                                                "477549",            "Link lane ID (middle)"},

    // Late section (around line 900,000)
    {         "Link.3485.LateralCenterLineOffset.ID",                                                                                                                                                                                                                                "894619",       "Link 3485 offset ID (late)"},
    {                   "Link.3485.LaneSection.0.ID",                                                                                                                                                                                                                                "894558",    "Link 3485 lane section (late)"},
    {                "Link.3485.LaneSection.0.Start",                                                                                                                                                                                                                                     "0",   "Link 3485 section start (late)"},

    // End of file
    {                          "Control.TrfLight.68",                                                                                                                                                                                    "941160 JuncArm_381952 Time>=0.000000 3 0 15 4 28 4",           "Traffic light 68 (end)"},
    {                          "Control.TrfLight.69",                                                                                                                                                                                                   "941161 CtrlTL015 \"\" 1 0 15 3 15 3",           "Traffic light 69 (end)"},
    {                                 "MaxUsedObjId",                                                                                                                                                                                                                                "941652",      "Max object ID (line 996366)"},

    // Multiline values
    {"Junction.0.Link.0.LaneSection.0.LaneL.0.Width",                                                                                                                                            "445061 -1 0 0 1 3.99495155267296 0 -999 -999\n362229 -1 0 1 1 3.78377990903144 0 -999 -999",        "Multiline width (2 lines)"},
    {"Junction.0.Link.0.LaneSection.0.LaneR.0.Width",                                                                                                                                            "445062 -1 0 0 1 4.00700290085828 0 -999 -999\n362227 -1 0 1 1 3.78039994814133 0 -999 -999",        "Multiline width (2 lines)"},
    {                     "Junction.1.HMMesh.DeltaU",                                                                                                                                                                                     "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1",        "Multiline DeltaU (1 line)"},
    {           "Junction.3.RL.3.Seg.0.Curve.Points", "362894 362565 -0.820901971666899 45.6687762230722 0 0 0 100 277.903092940284 -999\n362895 362565 3.2474857365014 16.4614153836155 0 0 0 100 -999 -999\n362896 362565 4.77912495504279 5.3511128462851 0 0 0 100 277.720834107522 -999", "Multiline curve points (3 lines)"},
};

// Test cases for result.erg.info (smaller file with detailed metadata)
static const TestCase erg_test_cases[] = {
    // File header
    {                                                   "File.Format",                                                        "erg",                     "File format"},
    {                                                "File.ByteOrder",                                               "LittleEndian",                      "Byte order"},
    {                                            "File.DateInSeconds",                                                 "1750288191",                 "Date in seconds"},
    {                                                "File.DateLocal",                        "2025-06-19 08:09:51 대한민국 표준시", "Local date with Korean timezone"},

    // File attributes (beginning)
    {                                                "File.At.1.Name",                                          "FCA_WrngLvlSta_CM",            "First attribute name"},
    {                                                "File.At.1.Type",                                                     "Double",            "First attribute type"},
    {                                                "File.At.2.Name",                                          "IDS.FCA_DclReqVal",           "Second attribute name"},
    {                                                "File.At.3.Name",                                                       "Time",                  "Time attribute"},
    {                                            "Quantity.Time.Unit",                                                          "s",                       "Time unit"},

    // Middle attributes
    {                                               "File.At.50.Name",           "Output_FR_C_Radar.GW_Radar_Object_00.motion_type",                    "Attribute 50"},
    {"Quantity.Output_FR_C_Radar.GW_Radar_Object_00.motion_type.Unit",                                                          "-",                "Motion type unit"},

    // Late attributes
    {                                              "File.At.100.Name",                       "Output_FR_Camera.Object_02.Rel_vel_X",                   "Attribute 100"},
    {                                              "File.At.133.Name",                            "Sensor.Collision.Vhcl.Fr1.Count",                  "Last attribute"},
    {                                              "File.At.133.Type",                                                        "Int",             "Last attribute type"},

    // Animation messages (multiline base64 data)
    {                                               "Anim.Msg.0.Time",                                                          "0",            "First animation time"},
    {                                              "Anim.Msg.0.Class",                                                       "Anim",           "First animation class"},
    {                                                 "Anim.Msg.0.Id",                                                         "18",              "First animation ID"},

    // Vehicle info
    {                                             "Anim.VehicleClass",                                                        "Car",                   "Vehicle class"},
    {                                        "Anim.Vehicle.MovieSkin",                                           "Kia_EV9_2023.obj",                    "Vehicle skin"},

    // Testrun info
    {                                                       "Testrun", "EuroNCAP_2026/Variations/AEB_CBLA/AEB_CBLA_30kph_15kph_50%",                    "Testrun path"},
    {                                               "SimParam.DeltaT",                                                      "0.001",           "Simulation delta time"},
    {                                                    "RandomSeed",                                                 "1750288116",                     "Random seed"},

    // Named values
    {                                             "NamedValues.Count",                                                          "0",              "Named values count"},
    {                                               "KeyValues.Count",                                                          "0",                "Key values count"},
    {                                              "GPUSensors.Count",                                                          "0",               "GPU sensors count"},

    // CarMaker version info
    {                                           "CarMaker.NumVersion",                                                     "120001",        "CarMaker numeric version"},
    {                                              "CarMaker.Version",                                                     "12.0.1",         "CarMaker version string"},
    {                                      "CarMaker.Version.MatSupp",                                                     "12.0.1",                 "MatSupp version"},
    {                                         "CarMaker.Version.Road",                                                     "12.0.1",                    "Road version"},
};

#define NUM_ROAD_CASES (sizeof(road_test_cases) / sizeof(road_test_cases[0]))
#define NUM_ERG_CASES  (sizeof(erg_test_cases) / sizeof(erg_test_cases[0]))

void test_basic_parsing() {
    printf("Testing basic parsing with inline data...\n");

    const char* test_data =
        "#INFOFILE1.1 (UTF-8) - Do not remove this line!\n"
        "\n"
        "File.Format = erg\n"
        "File.ByteOrder = LittleEndian\n"
        "File.DateInSeconds = 1750288191\n"
        "\n"
        "Comment:\n"
        "\tThis is a multiline comment\n"
        "\tWith multiple lines\n"
        "\n"
        "Anim.Msg.0.Data:\n"
        "\tline1\n"
        "\tline2\n"
        "\tline3\n";

    InfoFile info;
    infofile_init(&info);

    infofile_parse_string(test_data, strlen(test_data), &info);
    assert(info.count == 5);

    // Test single-line values
    const char* format = infofile_get(&info, "File.Format");
    assert(format != NULL);
    assert(strcmp(format, "erg") == 0);

    const char* byte_order = infofile_get(&info, "File.ByteOrder");
    assert(byte_order != NULL);
    assert(strcmp(byte_order, "LittleEndian") == 0);

    // Test multiline values
    const char* comment = infofile_get(&info, "Comment");
    assert(comment != NULL);
    assert(strstr(comment, "This is a multiline comment") != NULL);
    assert(strstr(comment, "With multiple lines") != NULL);

    const char* data = infofile_get(&info, "Anim.Msg.0.Data");
    assert(data != NULL);
    assert(strstr(data, "line1") != NULL);
    assert(strstr(data, "line2") != NULL);
    assert(strstr(data, "line3") != NULL);

    infofile_free(&info);
    printf("[OK] Basic parsing test passed (5 entries)\n");
}

void test_special_characters() {
    printf("Testing special characters in values...\n");

    const char* test_data =
        "Key.With.Equals = Value with = sign\n"
        "Key.With.Colon = Value with : sign\n"
        "Unicode.Test = 대한민국 표준시\n"
        "Mixed:\n"
        "\tValue = with : special = chars\n";

    InfoFile info;
    infofile_init(&info);

    infofile_parse_string(test_data, strlen(test_data), &info);

    const char* val1 = infofile_get(&info, "Key.With.Equals");
    assert(val1 != NULL);
    assert(strcmp(val1, "Value with = sign") == 0);

    const char* val2 = infofile_get(&info, "Key.With.Colon");
    assert(val2 != NULL);
    assert(strcmp(val2, "Value with : sign") == 0);

    const char* unicode = infofile_get(&info, "Unicode.Test");
    assert(unicode != NULL);
    assert(strcmp(unicode, "대한민국 표준시") == 0);

    const char* mixed = infofile_get(&info, "Mixed");
    assert(mixed != NULL);
    assert(strstr(mixed, "Value = with : special = chars") != NULL);

    infofile_free(&info);
    printf("[OK] Special characters test passed (4 entries)\n");
}

void test_file_comprehensive(const char* filename, const TestCase* test_cases, size_t num_cases, const char* file_desc) {
    printf("\nTesting %s...\n", file_desc);

    InfoFile info;
    infofile_init(&info);

    infofile_parse_file(filename, &info);

    printf("  Parsed %zu entries from %s\n", info.count, filename);

    // Test all test cases
    size_t passed = 0;
    for (size_t i = 0; i < num_cases; i++) {
        const char* value = infofile_get(&info, test_cases[i].key);
        if (value == NULL) {
            fprintf(stderr, "ERROR: Key '%s' not found in file\n", test_cases[i].key);
            fprintf(stderr, "       %s\n", test_cases[i].description);
            infofile_free(&info);
            exit(1);
        }

        if (strcmp(value, test_cases[i].expected_value) != 0) {
            fprintf(stderr, "ERROR: %s\n", test_cases[i].description);
            fprintf(stderr, "       Key: %s\n", test_cases[i].key);
            fprintf(stderr, "       Expected: '%s'\n", test_cases[i].expected_value);
            fprintf(stderr, "       Got:      '%s'\n", value);
            infofile_free(&info);
            exit(1);
        }
        passed++;
    }

    infofile_free(&info);
    printf("[OK] All %zu test cases passed for %s\n", passed, file_desc);
}

int main(int argc, char* argv[]) {
    printf("=== InfoFile Parser Comprehensive Test ===\n\n");

    // Test basic parsing with inline data
    test_basic_parsing();
    printf("\n");

    // Test special characters
    test_special_characters();

    // Determine file paths
    const char* road_file = NULL;
    const char* erg_file  = NULL;

    if (argc > 1) {
        road_file = argv[1];
    } else {
        // Try to find road.rd5
        FILE* test = fopen("example/road.rd5", "rb");
        if (test) {
            fclose(test);
            road_file = "example/road.rd5";
        } else {
            test = fopen("../example/road.rd5", "rb");
            if (test) {
                fclose(test);
                road_file = "../example/road.rd5";
            }
        }
    }

    if (argc > 2) {
        erg_file = argv[2];
    } else {
        // Try to find result.erg.info
        FILE* test = fopen("example/result.erg.info", "rb");
        if (test) {
            fclose(test);
            erg_file = "example/result.erg.info";
        } else {
            test = fopen("../example/result.erg.info", "rb");
            if (test) {
                fclose(test);
                erg_file = "../example/result.erg.info";
            }
        }
    }

    // Test road.rd5 if available
    if (road_file) {
        test_file_comprehensive(road_file, road_test_cases, NUM_ROAD_CASES, "road.rd5 (large file)");
    } else {
        printf("\n⚠ road.rd5 not found - skipping large file tests\n");
        printf("  (Place file in example/road.rd5 or pass path as first argument)\n");
    }

    // Test result.erg.info if available
    if (erg_file) {
        test_file_comprehensive(erg_file, erg_test_cases, NUM_ERG_CASES, "result.erg.info (detailed metadata)");
    } else {
        printf("\n⚠ result.erg.info not found - skipping metadata tests\n");
        printf("  (Place file in example/result.erg.info or pass path as second argument)\n");
    }

    printf("\n=== All tests passed! ===\n");

    if (road_file && erg_file) {
        printf("Tested %zu cases across 2 files plus basic parsing\n", NUM_ROAD_CASES + NUM_ERG_CASES);
    }

    return 0;
}
