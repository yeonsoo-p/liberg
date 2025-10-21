#include <infofile_arena.h>
#include <arena.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define INITIAL_CAPACITY 64
#define INITIAL_ARENA_SIZE (256 * 1024)  // 256KB initial arena
#define BUFFER_SIZE 4096

void infofile_arena_init(InfoFileArena *info) {
    info->entries = malloc(INITIAL_CAPACITY * sizeof(InfoFileEntryArena));
    info->count = 0;
    info->capacity = INITIAL_CAPACITY;
    arena_init(&info->arena, INITIAL_ARENA_SIZE);
}

static void ensure_capacity(InfoFileArena *info) {
    if (info->count >= info->capacity) {
        info->capacity *= 2;
        info->entries = realloc(info->entries, info->capacity * sizeof(InfoFileEntryArena));
    }
}

static char *trim_whitespace_arena(Arena *arena, const char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return arena_strdup(arena, "");

    const char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    size_t len = end - str + 1;
    return arena_strndup(arena, str, len);
}

static bool is_continuation_line(const char *line) {
    return line[0] == '\t' || (line[0] == ' ' && line[1] != '\0');
}

int infofile_arena_parse_string(const char *data, size_t len, InfoFileArena *info) {
    const char *ptr = data;
    const char *end = data + len;
    char line_buffer[BUFFER_SIZE];

    const char *current_key = NULL;
    char *value_buffer = NULL;
    size_t value_buffer_size = 0;
    size_t value_buffer_used = 0;
    bool is_multiline = false;

    while (ptr < end) {
        // Read a line
        size_t line_len = 0;
        while (ptr < end && *ptr != '\n' && line_len < BUFFER_SIZE - 1) {
            line_buffer[line_len++] = *ptr++;
        }
        line_buffer[line_len] = '\0';

        if (ptr < end && *ptr == '\n') {
            ptr++;
        }

        // Remove \r if present
        if (line_len > 0 && line_buffer[line_len - 1] == '\r') {
            line_buffer[--line_len] = '\0';
        }

        // Skip empty lines and comments (but not during multiline values)
        if (!is_multiline) {
            // Check without allocating from arena
            const char *check_ptr = line_buffer;
            while (isspace((unsigned char)*check_ptr)) check_ptr++;
            if (*check_ptr == '\0' || *check_ptr == '#') {
                continue;
            }
        }

        // Check if this is a continuation line
        if (is_continuation_line(line_buffer)) {
            if (is_multiline) {
                // Append to current multiline value buffer
                char *trimmed_content = trim_whitespace_arena(&info->arena, line_buffer);
                size_t trimmed_len = strlen(trimmed_content);
                size_t needed = trimmed_len + 2; // +1 for newline, +1 for null

                if (value_buffer_used + needed > value_buffer_size) {
                    size_t new_size = value_buffer_size ? value_buffer_size * 2 : 1024;
                    while (new_size < value_buffer_used + needed) new_size *= 2;
                    value_buffer = realloc(value_buffer, new_size);
                    value_buffer_size = new_size;
                }

                if (value_buffer_used > 0) {
                    value_buffer[value_buffer_used++] = '\n';
                }
                memcpy(value_buffer + value_buffer_used, trimmed_content, trimmed_len);
                value_buffer_used += trimmed_len;
                value_buffer[value_buffer_used] = '\0';
            }
            continue;
        }

        // If we have a pending multiline value, save it
        if (current_key != NULL) {
            ensure_capacity(info);
            info->entries[info->count].key = current_key;
            if (value_buffer) {
                info->entries[info->count].value = arena_strdup(&info->arena, value_buffer);
            } else {
                info->entries[info->count].value = arena_strdup(&info->arena, "");
            }
            info->count++;
            current_key = NULL;
            if (value_buffer) {
                value_buffer_used = 0;
                value_buffer[0] = '\0';
            }
            is_multiline = false;
        }

        // Parse key-value separator directly on line_buffer to avoid multiple arena allocations
        char *separator = strchr(line_buffer, '=');
        char *colon = strchr(line_buffer, ':');

        bool use_equals = false;
        if (separator != NULL && colon != NULL) {
            use_equals = (separator < colon);
        } else if (separator != NULL) {
            use_equals = true;
        } else if (colon != NULL) {
            use_equals = false;
        } else {
            continue;
        }

        if (use_equals) {
            // Single-line format
            *separator = '\0';
            current_key = trim_whitespace_arena(&info->arena, line_buffer);
            const char *value = trim_whitespace_arena(&info->arena, separator + 1);

            ensure_capacity(info);
            info->entries[info->count].key = current_key;
            info->entries[info->count].value = value;
            info->count++;
            current_key = NULL;
        } else {
            // Multi-line format
            *colon = '\0';
            current_key = trim_whitespace_arena(&info->arena, line_buffer);

            char *after_colon = trim_whitespace_arena(&info->arena, colon + 1);

            if (!value_buffer) {
                value_buffer_size = 1024;
                value_buffer = malloc(value_buffer_size);
            }
            value_buffer_used = 0;

            if (after_colon[0] != '\0') {
                size_t len = strlen(after_colon);
                if (len + 1 > value_buffer_size) {
                    value_buffer_size = len + 1024;
                    value_buffer = realloc(value_buffer, value_buffer_size);
                }
                memcpy(value_buffer, after_colon, len + 1);
                value_buffer_used = len;
            } else {
                value_buffer[0] = '\0';
            }

            is_multiline = true;
        }
    }

    // Save any pending value
    if (current_key != NULL) {
        ensure_capacity(info);
        info->entries[info->count].key = current_key;
        if (value_buffer && value_buffer_used > 0) {
            info->entries[info->count].value = arena_strdup(&info->arena, value_buffer);
        } else {
            info->entries[info->count].value = arena_strdup(&info->arena, "");
        }
        info->count++;
    }

    if (value_buffer) {
        free(value_buffer);
    }

    return 0;
}

int infofile_arena_parse_file(const char *filename, InfoFileArena *info) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    // Pre-allocate arena chunks to minimize allocations during parsing
    // Estimate: file size * 2 should be enough for trimmed strings
    // With chunk-based arena, this is safe - never invalidates pointers
    size_t estimated_arena_size = file_size * 2;
    arena_reserve(&info->arena, estimated_arena_size);

    int result = infofile_arena_parse_string(buffer, bytes_read, info);
    free(buffer);

    return result;
}

const char *infofile_arena_get(const InfoFileArena *info, const char *key) {
    for (size_t i = 0; i < info->count; i++) {
        if (strcmp(info->entries[i].key, key) == 0) {
            return info->entries[i].value;
        }
    }
    return NULL;
}

void infofile_arena_free(InfoFileArena *info) {
    free(info->entries);
    arena_free(&info->arena);
    info->entries = NULL;
    info->count = 0;
    info->capacity = 0;
}
