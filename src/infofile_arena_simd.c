#include <infofile_arena_simd.h>
#include <arena.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <immintrin.h>  // AVX2 intrinsics

#define INITIAL_CAPACITY 64
#define INITIAL_ARENA_SIZE (256 * 1024)  // 256KB initial arena
#define BUFFER_SIZE 4096

/* No longer need safety wrappers - chunk-based arena never invalidates pointers! */

void infofile_arena_simd_init(InfoFileArenaSimd *info) {
    info->entries = malloc(INITIAL_CAPACITY * sizeof(InfoFileEntryArenaSimd));
    info->count = 0;
    info->capacity = INITIAL_CAPACITY;
    arena_init(&info->arena, INITIAL_ARENA_SIZE);
}

static void ensure_capacity(InfoFileArenaSimd *info) {
    if (info->count >= info->capacity) {
        info->capacity *= 2;
        info->entries = realloc(info->entries, info->capacity * sizeof(InfoFileEntryArenaSimd));
    }
}

/* SIMD Helper: Find newline character using AVX2 */
static inline const char *simd_find_newline(const char *ptr, const char *end) {
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
                int offset = __builtin_ctz(mask);
                return ptr + offset;
            }
            ptr += 32;
        }
    }

    // Scalar fallback
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

            if (mask_hash != 0) {
                int offset = __builtin_ctz(mask_hash);
                *sep_pos = (char*)(ptr + offset);
                return '#';
            }

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

    // Scalar fallback
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

/* In-place trim: returns start and end pointers */
static inline void trim_in_place(const char *str, size_t len, const char **start, const char **end) {
    const char *s = str;
    const char *e = str + len - 1;

    while (s <= e && isspace((unsigned char)*s)) s++;
    while (e >= s && isspace((unsigned char)*e)) e--;

    *start = s;
    *end = e;
}

/* Allocate trimmed string from arena */
static const char *arena_alloc_trimmed(Arena *arena, const char *str, size_t len) {
    const char *start, *end;
    trim_in_place(str, len, &start, &end);

    if (start > end) {
        return arena_strndup(arena, "", 0);
    }

    size_t result_len = end - start + 1;
    return arena_strndup(arena, start, result_len);
}

static inline bool is_continuation_line(const char *line) {
    return line[0] == '\t' || (line[0] == ' ' && line[1] != '\0');
}

int infofile_arena_simd_parse_string(const char *data, size_t len, InfoFileArenaSimd *info) {
    const char *ptr = data;
    const char *end = data + len;
    char line_buffer[BUFFER_SIZE];

    const char *current_key = NULL;
    char *value_buffer = NULL;
    size_t value_buffer_size = 0;
    size_t value_buffer_used = 0;
    bool is_multiline = false;

    while (ptr < end) {
        // Use SIMD to find next newline
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
            line_len = end - ptr;
            if (line_len >= BUFFER_SIZE) {
                line_len = BUFFER_SIZE - 1;
            }
            memcpy(line_buffer, ptr, line_len);
            line_buffer[line_len] = '\0';
            ptr = end;
        }

        // Remove \r if present
        if (line_len > 0 && line_buffer[line_len - 1] == '\r') {
            line_buffer[--line_len] = '\0';
        }

        // Quick check for empty lines and comments
        if (!is_multiline) {
            const char *check_ptr = line_buffer;
            while (isspace((unsigned char)*check_ptr)) check_ptr++;
            if (*check_ptr == '\0' || *check_ptr == '#') {
                continue;
            }
        }

        // Check if this is a continuation line
        if (is_continuation_line(line_buffer)) {
            if (is_multiline) {
                // Append to multiline value buffer
                const char *trim_start, *trim_end;
                trim_in_place(line_buffer, line_len, &trim_start, &trim_end);

                if (trim_start <= trim_end) {
                    size_t trimmed_len = trim_end - trim_start + 1;
                    size_t needed = trimmed_len + 2;

                    if (value_buffer_used + needed > value_buffer_size) {
                        size_t new_size = value_buffer_size ? value_buffer_size * 2 : 1024;
                        while (new_size < value_buffer_used + needed) new_size *= 2;
                        value_buffer = realloc(value_buffer, new_size);
                        value_buffer_size = new_size;
                    }

                    if (value_buffer_used > 0) {
                        value_buffer[value_buffer_used++] = '\n';
                    }
                    memcpy(value_buffer + value_buffer_used, trim_start, trimmed_len);
                    value_buffer_used += trimmed_len;
                    value_buffer[value_buffer_used] = '\0';
                }
            }
            continue;
        }

        // If we have a pending multiline value, save it
        if (current_key != NULL) {
            ensure_capacity(info);
            info->entries[info->count].key = current_key;
            if (value_buffer && value_buffer_used > 0) {
                info->entries[info->count].value = arena_strndup(&info->arena, value_buffer, value_buffer_used);
            } else {
                info->entries[info->count].value = arena_strndup(&info->arena, "", 0);
            }
            info->count++;
            current_key = NULL;
            if (value_buffer) {
                value_buffer_used = 0;
                value_buffer[0] = '\0';
            }
            is_multiline = false;
        }

        // Use SIMD to find separator
        char *sep_pos;
        char sep_char = simd_find_separator(line_buffer, line_len, &sep_pos);

        if (sep_char == '\0' || sep_char == '#') {
            continue;
        }

        if (sep_char == '=') {
            // Single-line format: key = value
            *sep_pos = '\0';
            current_key = arena_alloc_trimmed(&info->arena, line_buffer, sep_pos - line_buffer);
            const char *value = arena_alloc_trimmed(&info->arena, sep_pos + 1, line_len - (sep_pos - line_buffer + 1));

            ensure_capacity(info);
            info->entries[info->count].key = current_key;
            info->entries[info->count].value = value;
            info->count++;
            current_key = NULL;
        } else if (sep_char == ':') {
            // Multi-line format: key:
            *sep_pos = '\0';
            current_key = arena_alloc_trimmed(&info->arena, line_buffer, sep_pos - line_buffer);

            // Check if there's content after the colon
            size_t after_len = line_len - (sep_pos - line_buffer + 1);
            if (after_len > 0) {
                const char *after_start, *after_end;
                trim_in_place(sep_pos + 1, after_len, &after_start, &after_end);

                if (after_start <= after_end) {
                    size_t content_len = after_end - after_start + 1;
                    if (!value_buffer) {
                        value_buffer_size = 1024;
                        value_buffer = malloc(value_buffer_size);
                    }
                    if (content_len + 1 > value_buffer_size) {
                        value_buffer_size = content_len + 1024;
                        value_buffer = realloc(value_buffer, value_buffer_size);
                    }
                    memcpy(value_buffer, after_start, content_len);
                    value_buffer[content_len] = '\0';
                    value_buffer_used = content_len;
                } else {
                    value_buffer_used = 0;
                    if (value_buffer) value_buffer[0] = '\0';
                }
            } else {
                value_buffer_used = 0;
                if (value_buffer) value_buffer[0] = '\0';
            }
            is_multiline = true;
        }
    }

    // Save any pending value
    if (current_key != NULL) {
        ensure_capacity(info);
        info->entries[info->count].key = current_key;
        if (value_buffer && value_buffer_used > 0) {
            info->entries[info->count].value = arena_strndup(&info->arena, value_buffer, value_buffer_used);
        } else {
            info->entries[info->count].value = arena_strndup(&info->arena, "", 0);
        }
        info->count++;
    }

    if (value_buffer) {
        free(value_buffer);
    }

    return 0;
}

int infofile_arena_simd_parse_file(const char *filename, InfoFileArenaSimd *info) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Pre-allocate entries array based on file size estimate
    size_t estimated_entries = file_size / 150;
    if (estimated_entries > info->capacity) {
        info->capacity = estimated_entries;
        info->entries = realloc(info->entries, info->capacity * sizeof(InfoFileEntryArenaSimd));
    }

    // Pre-allocate arena to avoid reallocation during parsing
    // Estimate: file size * 2 should be enough
    size_t estimated_arena_size = file_size * 2;
    arena_reserve(&info->arena, estimated_arena_size);

    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    int result = infofile_arena_simd_parse_string(buffer, bytes_read, info);
    free(buffer);

    return result;
}

const char *infofile_arena_simd_get(const InfoFileArenaSimd *info, const char *key) {
    for (size_t i = 0; i < info->count; i++) {
        if (strcmp(info->entries[i].key, key) == 0) {
            return info->entries[i].value;
        }
    }
    return NULL;
}

void infofile_arena_simd_free(InfoFileArenaSimd *info) {
    free(info->entries);
    arena_free(&info->arena);
    info->entries = NULL;
    info->count = 0;
    info->capacity = 0;
}
