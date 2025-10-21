#include <erginfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void test_basic_parsing() {
    printf("Testing basic parsing...\n");

    const char *test_data =
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

    ErgInfo info;
    erginfo_init(&info);

    int result = erginfo_parse_string(test_data, strlen(test_data), &info);
    assert(result == 0);
    printf("  Parsed %zu entries\n", info.count);
    for (size_t i = 0; i < info.count; i++) {
        printf("  [%zu] %s = %s\n", i, info.entries[i].key, info.entries[i].value);
    }
    assert(info.count == 5);

    // Test single-line values
    const char *format = erginfo_get(&info, "File.Format");
    assert(format != NULL);
    assert(strcmp(format, "erg") == 0);

    const char *byte_order = erginfo_get(&info, "File.ByteOrder");
    assert(byte_order != NULL);
    assert(strcmp(byte_order, "LittleEndian") == 0);

    // Test multiline values
    const char *comment = erginfo_get(&info, "Comment");
    assert(comment != NULL);
    assert(strstr(comment, "This is a multiline comment") != NULL);
    assert(strstr(comment, "With multiple lines") != NULL);

    const char *data = erginfo_get(&info, "Anim.Msg.0.Data");
    assert(data != NULL);
    assert(strstr(data, "line1") != NULL);
    assert(strstr(data, "line2") != NULL);
    assert(strstr(data, "line3") != NULL);

    erginfo_free(&info);
    printf("✓ Basic parsing test passed\n");
}

void test_set_and_get() {
    printf("Testing set and get...\n");

    ErgInfo info;
    erginfo_init(&info);

    // Add new entries
    erginfo_set(&info, "Test.Key1", "Value1");
    erginfo_set(&info, "Test.Key2", "Value2");

    const char *val1 = erginfo_get(&info, "Test.Key1");
    assert(val1 != NULL);
    assert(strcmp(val1, "Value1") == 0);

    // Update existing entry
    erginfo_set(&info, "Test.Key1", "UpdatedValue");
    val1 = erginfo_get(&info, "Test.Key1");
    assert(strcmp(val1, "UpdatedValue") == 0);

    erginfo_free(&info);
    printf("✓ Set and get test passed\n");
}

void test_write_string() {
    printf("Testing write to string...\n");

    ErgInfo info;
    erginfo_init(&info);

    erginfo_set(&info, "Simple.Key", "SimpleValue");
    erginfo_set(&info, "Multi.Line", "Line1\nLine2\nLine3");

    char *output = erginfo_write_string(&info);
    assert(output != NULL);

    // Check that output contains expected format
    assert(strstr(output, "Simple.Key = SimpleValue") != NULL);
    assert(strstr(output, "Multi.Line:") != NULL);
    assert(strstr(output, "\tLine1") != NULL);
    assert(strstr(output, "\tLine2") != NULL);

    free(output);
    erginfo_free(&info);
    printf("✓ Write to string test passed\n");
}

void test_special_characters() {
    printf("Testing special characters in values...\n");

    const char *test_data =
        "Key.With.Equals = Value with = sign\n"
        "Key.With.Colon = Value with : sign\n"
        "Unicode.Test = 대한민국 표준시\n"
        "Mixed:\n"
        "\tValue = with : special = chars\n";

    ErgInfo info;
    erginfo_init(&info);

    int result = erginfo_parse_string(test_data, strlen(test_data), &info);
    assert(result == 0);

    const char *val1 = erginfo_get(&info, "Key.With.Equals");
    assert(val1 != NULL);
    assert(strcmp(val1, "Value with = sign") == 0);

    const char *val2 = erginfo_get(&info, "Key.With.Colon");
    assert(val2 != NULL);
    assert(strcmp(val2, "Value with : sign") == 0);

    const char *unicode = erginfo_get(&info, "Unicode.Test");
    assert(unicode != NULL);
    assert(strcmp(unicode, "대한민국 표준시") == 0);

    const char *mixed = erginfo_get(&info, "Mixed");
    assert(mixed != NULL);
    assert(strstr(mixed, "Value = with : special = chars") != NULL);

    erginfo_free(&info);
    printf("✓ Special characters test passed\n");
}

void test_real_file(const char *filename) {
    if (!filename) {
        printf("Testing with real erg.info file...\n");
        printf("⚠ No file specified (use ./test_erginfo <file> to test with real file)\n");
        return;
    }

    printf("Testing with real erg.info file: %s...\n", filename);

    ErgInfo info;
    erginfo_init(&info);

    int result = erginfo_parse_file(filename, &info);

    if (result == 0) {
        printf("  Parsed %zu entries from real file\n", info.count);

        // Test a few known values
        const char *format = erginfo_get(&info, "File.Format");
        if (format) {
            printf("  File.Format = %s\n", format);
            assert(strcmp(format, "erg") == 0);
        }

        const char *testrun = erginfo_get(&info, "Testrun");
        if (testrun) {
            printf("  Testrun = %s\n", testrun);
        }

        const char *comment = erginfo_get(&info, "Comment");
        if (comment) {
            printf("  Comment (first 50 chars): %.50s...\n", comment);
        }

        erginfo_free(&info);
        printf("✓ Real file test passed\n");
    } else {
        printf("⚠ Could not open real file (this is OK if file doesn't exist)\n");
    }
}

int main(int argc, char *argv[]) {
    printf("=== ERG Info Parser Tests ===\n\n");

    test_basic_parsing();
    test_set_and_get();
    test_write_string();
    test_special_characters();
    test_real_file(argc > 1 ? argv[1] : NULL);

    printf("\n=== All tests passed! ===\n");
    return 0;
}
