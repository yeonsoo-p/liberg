#include <infofile_simd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <immintrin.h>  // AVX2 intrinsics

#define INITIAL_CAPACITY 64
#define BUFFER_SIZE 4096

void infofile_simd_init(InfoFileSIMD *info) {
    info->entries = malloc(INITIAL_CAPACITY * sizeof(InfoFileEntrySIMD));
    info->count = 0;
    info->capacity = INITIAL_CAPACITY;
}

static void ensure_capacity(InfoFileSIMD *info) {
    if (info->count >= info->capacity) {
        info->capacity *= 2;
        info->entries = realloc(info->entries, info->capacity * sizeof(InfoFileEntrySIMD));
    }
}

/* SIMD Helper: Find newline character using AVX2 */
static inline const char *simd_find_newline(const char *ptr, const char *end) {
    const char *orig_ptr = ptr;
    size_t len = end - ptr;

    // Use AVX2 for chunks of 32 bytes
    if (len >= 32) {
        __m256i newline_vec = _mm256_set1_epi8('\n');

        // Process 32-byte chunks
        while (ptr + 32 <= end) {
            __m256i data = _mm256_loadu_si256((const __m256i*)ptr);
            __m256i cmp = _mm256_cmpeq_epi8(data, newline_vec);
            int mask = _mm256_movemask_epi8(cmp);

            if (mask != 0) {
                // Found newline - find first set bit
                int offset = __builtin_ctz(mask);
                return ptr + offset;
            }
            ptr += 32;
        }
    }

    // Handle remaining bytes with scalar code
    while (ptr < end) {
        if (*ptr == '\n') {
            return ptr;
        }
        ptr++;
    }

    return NULL;
}

/* SIMD Helper: Find separator ('=' or ':') or comment ('#') */
static inline char simd_find_separator(const char *str, size_t len, char **sep_pos) {
    const char *ptr = str;
    const char *end = str + len;

    // Use AVX2 for chunks of 32 bytes
    if (len >= 32) {
        __m256i equals_vec = _mm256_set1_epi8('=');
        __m256i colon_vec = _mm256_set1_epi8(':');
        __m256i hash_vec = _mm256_set1_epi8('#');

        while (ptr + 32 <= end) {
            __m256i data = _mm256_loadu_si256((const __m256i*)ptr);
            __m256i cmp_eq = _mm256_cmpeq_epi8(data, equals_vec);
            __m256i cmp_colon = _mm256_cmpeq_epi8(data, colon_vec);
            __m256i cmp_hash = _mm256_cmpeq_epi8(data, hash_vec);

            int mask_eq = _mm256_movemask_epi8(cmp_eq);
            int mask_colon = _mm256_movemask_epi8(cmp_colon);
            int mask_hash = _mm256_movemask_epi8(cmp_hash);

            // Check for comment first
            if (mask_hash != 0) {
                int offset = __builtin_ctz(mask_hash);
                *sep_pos = (char*)(ptr + offset);
                return '#';
            }

            // Check which separator comes first
            if (mask_eq != 0 && mask_colon != 0) {
                int offset_eq = __builtin_ctz(mask_eq);
                int offset_colon = __builtin_ctz(mask_colon);
                if (offset_eq < offset_colon) {
                    *sep_pos = (char*)(ptr + offset_eq);
                    return '=';
                } else {
                    *sep_pos = (char*)(ptr + offset_colon);
                    return ':';
                }
            } else if (mask_eq != 0) {
                int offset = __builtin_ctz(mask_eq);
                *sep_pos = (char*)(ptr + offset);
                return '=';
            } else if (mask_colon != 0) {
                int offset = __builtin_ctz(mask_colon);
                *sep_pos = (char*)(ptr + offset);
                return ':';
            }

            ptr += 32;
        }
    }

    // Scalar fallback for remaining bytes
    while (ptr < end) {
        if (*ptr == '#') {
            *sep_pos = (char*)ptr;
            return '#';
        }
        if (*ptr == '=') {
            *sep_pos = (char*)ptr;
            return '=';
        }
        if (*ptr == ':') {
            *sep_pos = (char*)ptr;
            return ':';
        }
        ptr++;
    }

    *sep_pos = NULL;
    return '\0';
}

/* Optimized in-place trim: returns start and end pointers */
static inline void trim_in_place(const char *str, size_t len, const char **start, const char **end) {
    const char *s = str;
    const char *e = str + len - 1;

    // Skip leading whitespace
    while (s <= e && isspace((unsigned char)*s)) s++;

    // Skip trailing whitespace
    while (e >= s && isspace((unsigned char)*e)) e--;

    *start = s;
    *end = e;
}

/* Allocate trimmed string */
static char *alloc_trimmed(const char *str, size_t len) {
    const char *start, *end;
    trim_in_place(str, len, &start, &end);

    if (start > end) {
        return strdup("");
    }

    size_t result_len = end - start + 1;
    char *result = malloc(result_len + 1);
    memcpy(result, start, result_len);
    result[result_len] = '\0';
    return result;
}

static inline bool is_continuation_line(const char *line) {
    return line[0] == '\t' || (line[0] == ' ' && line[1] != '\0');
}

int infofile_simd_parse_string(const char *data, size_t len, InfoFileSIMD *info) {
    const char *ptr = data;
    const char *end = data + len;
    char line_buffer[BUFFER_SIZE];

    char *current_key = NULL;
    char *current_value = NULL;
    bool is_multiline = false;

    while (ptr < end) {
        // Use SIMD to find next newline
        const char *line_start = ptr;
        const char *newline_pos = simd_find_newline(ptr, end);

        size_t line_len;
        if (newline_pos) {
            line_len = newline_pos - ptr;
            if (line_len >= BUFFER_SIZE) {
                line_len = BUFFER_SIZE - 1;
            }
            memcpy(line_buffer, ptr, line_len);
            line_buffer[line_len] = '\0';
            ptr = newline_pos + 1;
        } else {
            // Last line without newline
            line_len = end - ptr;
            if (line_len >= BUFFER_SIZE) {
                line_len = BUFFER_SIZE - 1;
            }
            memcpy(line_buffer, ptr, line_len);
            line_buffer[line_len] = '\0';
            ptr = end;
        }

        // Remove \r if present (Windows line endings)
        if (line_len > 0 && line_buffer[line_len - 1] == '\r') {
            line_buffer[--line_len] = '\0';
        }

        // Quick check for empty lines and comments
        const char *check_start, *check_end;
        trim_in_place(line_buffer, line_len, &check_start, &check_end);

        if (check_start > check_end || *check_start == '#') {
            continue;
        }

        // Check if this is a continuation line
        if (is_continuation_line(line_buffer)) {
            if (is_multiline && current_value != NULL) {
                // Append to current multiline value
                char *trimmed_content = alloc_trimmed(line_buffer, line_len);
                size_t old_len = strlen(current_value);
                size_t new_len = strlen(trimmed_content);
                current_value = realloc(current_value, old_len + new_len + 2);
                current_value[old_len] = '\n';
                strcpy(current_value + old_len + 1, trimmed_content);
                free(trimmed_content);
            }
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

        // Use SIMD to find separator
        char *sep_pos;
        char sep_char = simd_find_separator(line_buffer, line_len, &sep_pos);

        if (sep_char == '\0' || sep_char == '#') {
            // No separator or comment line
            continue;
        }

        if (sep_char == '=') {
            // Single-line format: key = value
            *sep_pos = '\0';
            current_key = alloc_trimmed(line_buffer, sep_pos - line_buffer);
            current_value = alloc_trimmed(sep_pos + 1, line_len - (sep_pos - line_buffer + 1));
            is_multiline = false;

            // Save immediately for single-line values
            ensure_capacity(info);
            info->entries[info->count].key = current_key;
            info->entries[info->count].value = current_value;
            info->count++;
            current_key = NULL;
            current_value = NULL;
        } else if (sep_char == ':') {
            // Multi-line format: key:
            *sep_pos = '\0';
            current_key = alloc_trimmed(line_buffer, sep_pos - line_buffer);

            // Check if there's content after the colon
            size_t after_len = line_len - (sep_pos - line_buffer + 1);
            if (after_len > 0) {
                const char *after_start, *after_end;
                trim_in_place(sep_pos + 1, after_len, &after_start, &after_end);
                if (after_start <= after_end) {
                    size_t content_len = after_end - after_start + 1;
                    current_value = malloc(content_len + 1);
                    memcpy(current_value, after_start, content_len);
                    current_value[content_len] = '\0';
                } else {
                    current_value = strdup("");
                }
            } else {
                current_value = strdup("");
            }
            is_multiline = true;
        }
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

int infofile_simd_parse_file(const char *filename, InfoFileSIMD *info) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return -1;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Smart pre-allocation: estimate entries from file size
    // Average is about 150 bytes per entry
    size_t estimated_entries = file_size / 150;
    if (estimated_entries > info->capacity) {
        info->capacity = estimated_entries;
        info->entries = realloc(info->entries, info->capacity * sizeof(InfoFileEntrySIMD));
    }

    // Read entire file
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    int result = infofile_simd_parse_string(buffer, bytes_read, info);
    free(buffer);

    return result;
}

const char *infofile_simd_get(const InfoFileSIMD *info, const char *key) {
    for (size_t i = 0; i < info->count; i++) {
        if (strcmp(info->entries[i].key, key) == 0) {
            return info->entries[i].value;
        }
    }
    return NULL;
}

int infofile_simd_set(InfoFileSIMD *info, const char *key, const char *value) {
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

char *infofile_simd_write_string(const InfoFileSIMD *info) {
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

int infofile_simd_write_file(const char *filename, const InfoFileSIMD *info) {
    char *content = infofile_simd_write_string(info);
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

void infofile_simd_free(InfoFileSIMD *info) {
    for (size_t i = 0; i < info->count; i++) {
        free(info->entries[i].key);
        free(info->entries[i].value);
    }
    free(info->entries);
    info->entries = NULL;
    info->count = 0;
    info->capacity = 0;
}
