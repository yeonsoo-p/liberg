#include <erginfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define INITIAL_CAPACITY 64
#define BUFFER_SIZE 4096

void erginfo_init(ErgInfo *info) {
    info->entries = malloc(INITIAL_CAPACITY * sizeof(ErgInfoEntry));
    info->count = 0;
    info->capacity = INITIAL_CAPACITY;
}

static void ensure_capacity(ErgInfo *info) {
    if (info->count >= info->capacity) {
        info->capacity *= 2;
        info->entries = realloc(info->entries, info->capacity * sizeof(ErgInfoEntry));
    }
}

static char *trim_whitespace(const char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return strdup("");

    const char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    size_t len = end - str + 1;
    char *result = malloc(len + 1);
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

static bool is_continuation_line(const char *line) {
    return line[0] == '\t' || (line[0] == ' ' && line[1] != '\0');
}

int erginfo_parse_string(const char *data, size_t len, ErgInfo *info) {
    const char *ptr = data;
    const char *end = data + len;
    char line_buffer[BUFFER_SIZE];

    char *current_key = NULL;
    char *current_value = NULL;
    bool is_multiline = false;

    while (ptr < end) {
        // Read a line
        size_t line_len = 0;
        const char *line_start = ptr;

        while (ptr < end && *ptr != '\n' && line_len < BUFFER_SIZE - 1) {
            line_buffer[line_len++] = *ptr++;
        }
        line_buffer[line_len] = '\0';

        if (ptr < end && *ptr == '\n') {
            ptr++;
        }

        // Remove \r if present (for Windows line endings)
        if (line_len > 0 && line_buffer[line_len - 1] == '\r') {
            line_buffer[--line_len] = '\0';
        }

        // Skip empty lines and comments
        char *trimmed = trim_whitespace(line_buffer);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            free(trimmed);
            continue;
        }

        // Check if this is a continuation line
        if (is_continuation_line(line_buffer)) {
            if (is_multiline && current_value != NULL) {
                // Append to current multiline value
                char *trimmed_content = trim_whitespace(line_buffer);
                size_t old_len = strlen(current_value);
                size_t new_len = strlen(trimmed_content);
                current_value = realloc(current_value, old_len + new_len + 2);
                current_value[old_len] = '\n';
                strcpy(current_value + old_len + 1, trimmed_content);
                free(trimmed_content);
            }
            free(trimmed);
            continue;
        }

        // If we have a pending multiline value, save it
        if (current_key != NULL) {
            ensure_capacity(info);
            info->entries[info->count].key = current_key;
            info->entries[info->count].value = current_value ? current_value : strdup("");
            info->count++;
            current_key = NULL;
            current_value = NULL;
            is_multiline = false;
        }

        // Parse key-value separator
        char *separator = strchr(trimmed, '=');
        char *colon = strchr(trimmed, ':');

        // Determine which separator comes first
        bool use_equals = false;
        if (separator != NULL && colon != NULL) {
            use_equals = (separator < colon);
        } else if (separator != NULL) {
            use_equals = true;
        } else if (colon != NULL) {
            use_equals = false;
        } else {
            // No separator found, skip this line
            free(trimmed);
            continue;
        }

        if (use_equals) {
            // Single-line format: key = value
            *separator = '\0';
            current_key = trim_whitespace(trimmed);
            current_value = trim_whitespace(separator + 1);
            is_multiline = false;

            // Save immediately for single-line values
            ensure_capacity(info);
            info->entries[info->count].key = current_key;
            info->entries[info->count].value = current_value;
            info->count++;
            current_key = NULL;
            current_value = NULL;
        } else {
            // Multi-line format: key:
            *colon = '\0';
            current_key = trim_whitespace(trimmed);

            // Check if there's content after the colon
            char *after_colon = trim_whitespace(colon + 1);
            if (after_colon[0] != '\0') {
                current_value = after_colon;
            } else {
                current_value = strdup("");
                free(after_colon);
            }
            is_multiline = true;
        }

        free(trimmed);
    }

    // Save any pending value
    if (current_key != NULL) {
        ensure_capacity(info);
        info->entries[info->count].key = current_key;
        info->entries[info->count].value = current_value ? current_value : strdup("");
        info->count++;
    }

    return 0;
}

int erginfo_parse_file(const char *filename, ErgInfo *info) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return -1;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Read entire file
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    int result = erginfo_parse_string(buffer, bytes_read, info);
    free(buffer);

    return result;
}

const char *erginfo_get(const ErgInfo *info, const char *key) {
    for (size_t i = 0; i < info->count; i++) {
        if (strcmp(info->entries[i].key, key) == 0) {
            return info->entries[i].value;
        }
    }
    return NULL;
}

int erginfo_set(ErgInfo *info, const char *key, const char *value) {
    // Check if key exists
    for (size_t i = 0; i < info->count; i++) {
        if (strcmp(info->entries[i].key, key) == 0) {
            // Update existing value
            free(info->entries[i].value);
            info->entries[i].value = strdup(value);
            return 0;
        }
    }

    // Add new entry
    ensure_capacity(info);
    info->entries[info->count].key = strdup(key);
    info->entries[info->count].value = strdup(value);
    info->count++;
    return 0;
}

char *erginfo_write_string(const ErgInfo *info) {
    size_t buffer_size = 4096;
    size_t buffer_used = 0;
    char *buffer = malloc(buffer_size);

    for (size_t i = 0; i < info->count; i++) {
        const char *key = info->entries[i].key;
        const char *value = info->entries[i].value;

        // Check if value contains newlines (multiline)
        bool is_multiline = strchr(value, '\n') != NULL;

        if (is_multiline) {
            // Write key:
            size_t needed = strlen(key) + 2; // key + ":\n"
            while (buffer_used + needed >= buffer_size) {
                buffer_size *= 2;
                buffer = realloc(buffer, buffer_size);
            }
            buffer_used += sprintf(buffer + buffer_used, "%s:\n", key);

            // Write indented value lines
            const char *line_start = value;
            while (*line_start) {
                const char *line_end = strchr(line_start, '\n');
                size_t line_len;

                if (line_end) {
                    line_len = line_end - line_start;
                } else {
                    line_len = strlen(line_start);
                }

                needed = line_len + 3; // tab + line + \n
                while (buffer_used + needed >= buffer_size) {
                    buffer_size *= 2;
                    buffer = realloc(buffer, buffer_size);
                }

                buffer[buffer_used++] = '\t';
                memcpy(buffer + buffer_used, line_start, line_len);
                buffer_used += line_len;
                buffer[buffer_used++] = '\n';

                if (line_end) {
                    line_start = line_end + 1;
                } else {
                    break;
                }
            }
        } else {
            // Write key = value
            size_t needed = strlen(key) + strlen(value) + 4; // key + " = " + value + "\n"
            while (buffer_used + needed >= buffer_size) {
                buffer_size *= 2;
                buffer = realloc(buffer, buffer_size);
            }
            buffer_used += sprintf(buffer + buffer_used, "%s = %s\n", key, value);
        }
    }

    buffer[buffer_used] = '\0';
    return buffer;
}

int erginfo_write_file(const char *filename, const ErgInfo *info) {
    char *content = erginfo_write_string(info);
    if (!content) {
        return -1;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(content);
        return -1;
    }

    fputs(content, fp);
    fclose(fp);
    free(content);

    return 0;
}

void erginfo_free(ErgInfo *info) {
    for (size_t i = 0; i < info->count; i++) {
        free(info->entries[i].key);
        free(info->entries[i].value);
    }
    free(info->entries);
    info->entries = NULL;
    info->count = 0;
    info->capacity = 0;
}
