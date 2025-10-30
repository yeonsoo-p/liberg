#include <arena.h>
#include <ctype.h>
#include <immintrin.h> // AVX2 intrinsics
#include <infofile.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY   64
#define INITIAL_ARENA_SIZE (256 * 1024) // 256KB initial arena

/* ============================================================================
 * OPTIMIZATION 1: Zero-Copy Parsing
 * Parse directly from file buffer without memcpy to line_buffer
 * ============================================================================ */

/* ============================================================================
 * OPTIMIZATION 4: Dual Arena Layout
 * Separate arenas for keys (hot) and values (cold) for better cache locality
 * ============================================================================ */

/* No longer need safety wrappers - chunk-based arena never invalidates pointers! */

void infofile_init(InfoFile* info) {
    info->entries  = malloc(INITIAL_CAPACITY * sizeof(InfoFileEntry));
    if (!info->entries) {
        fprintf(stderr, "FATAL: Failed to allocate initial entries array (%zu bytes)\n",
                INITIAL_CAPACITY * sizeof(InfoFileEntry));
        exit(1);
    }
    info->count    = 0;
    info->capacity = INITIAL_CAPACITY;
    arena_init(&info->arena.key_arena, INITIAL_ARENA_SIZE);
    arena_init(&info->arena.value_arena, INITIAL_ARENA_SIZE);
}

static void ensure_capacity(InfoFile* info) {
    if (info->count >= info->capacity) {
        size_t new_capacity = info->capacity * 2;
        InfoFileEntry* new_entries = realloc(info->entries, new_capacity * sizeof(InfoFileEntry));
        if (!new_entries) {
            fprintf(stderr, "FATAL: Failed to reallocate entries array (%zu bytes)\n",
                    new_capacity * sizeof(InfoFileEntry));
            exit(1);
        }
        info->entries = new_entries;
        info->capacity = new_capacity;
    }
}

/* ============================================================================
 * SIMD Helper: Find newline character using AVX2
 * ============================================================================ */
static inline const char* simd_find_newline_opt(const char* ptr, const char* end) {
    size_t len = end - ptr;

    if (len >= 32) {
        __m256i newline_vec = _mm256_set1_epi8('\n');

        while (ptr + 32 <= end) {
            __m256i data = _mm256_loadu_si256((const __m256i*)ptr);
            __m256i cmp  = _mm256_cmpeq_epi8(data, newline_vec);
            int     mask = _mm256_movemask_epi8(cmp);

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

/* ============================================================================
 * OPTIMIZATION 2: SIMD Whitespace Trimming
 * Use AVX2 to detect whitespace 32 bytes at a time
 * ============================================================================ */

static inline const char* simd_skip_leading_whitespace(const char* str, const char* end) {
    size_t len = end - str;

    if (len >= 32) {
        __m256i space = _mm256_set1_epi8(' ');
        __m256i tab   = _mm256_set1_epi8('\t');
        __m256i cr    = _mm256_set1_epi8('\r');
        __m256i nl    = _mm256_set1_epi8('\n');

        const char* ptr = str;
        while (ptr + 32 <= end) {
            __m256i data = _mm256_loadu_si256((const __m256i*)ptr);

            // Check if any byte is NOT whitespace
            __m256i cmp_space = _mm256_cmpeq_epi8(data, space);
            __m256i cmp_tab   = _mm256_cmpeq_epi8(data, tab);
            __m256i cmp_cr    = _mm256_cmpeq_epi8(data, cr);
            __m256i cmp_nl    = _mm256_cmpeq_epi8(data, nl);

            // OR all whitespace comparisons
            __m256i is_ws1        = _mm256_or_si256(cmp_space, cmp_tab);
            __m256i is_ws2        = _mm256_or_si256(cmp_cr, cmp_nl);
            __m256i is_whitespace = _mm256_or_si256(is_ws1, is_ws2);

            int mask = _mm256_movemask_epi8(is_whitespace);

            // If not all whitespace, find first non-whitespace
            if (mask != 0xFFFFFFFF) {
                // Invert mask to find non-whitespace
                int non_ws_mask = ~mask;
                if (non_ws_mask != 0) {
                    int offset = __builtin_ctz(non_ws_mask);
                    return ptr + offset;
                }
            }

            ptr += 32;
        }
    }

    // Scalar fallback
    const char* ptr = str;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n')) {
        ptr++;
    }
    return ptr;
}

static inline const char* simd_skip_trailing_whitespace(const char* str, const char* end) {
    // Work backwards from end
    const char* ptr = end - 1;

    // Scalar approach for trailing (harder to vectorize backwards efficiently)
    while (ptr >= str && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n')) {
        ptr--;
    }

    return ptr + 1;
}

/* Zero-copy trim: returns trimmed range within original buffer */
static inline void simd_trim_zero_copy(const char* str, const char* str_end,
                                       const char** start, const char** end) {
    const char* s = simd_skip_leading_whitespace(str, str_end);
    if (s >= str_end) {
        *start = str_end;
        *end   = str_end;
        return;
    }

    const char* e = simd_skip_trailing_whitespace(str, str_end);
    *start        = s;
    *end          = e;
}

/* ============================================================================
 * OPTIMIZATION 3: SIMD Character Class Detection
 * Find separators, whitespace, and comments in single pass
 * ============================================================================ */

typedef struct
{
    char        sep_char;       // '=', ':', '#', or '\0'
    const char* sep_pos;        // Position of separator
    bool        has_leading_ws; // True if line starts with whitespace
} SeparatorInfo;

static inline void simd_find_special_chars(const char* str, const char* str_end,
                                           SeparatorInfo* info) {
    const char* ptr = str;
    size_t      len = str_end - str;

    info->sep_char       = '\0';
    info->sep_pos        = NULL;
    info->has_leading_ws = false;

    // Check first character for whitespace (continuation line check)
    if (len > 0 && (*ptr == ' ' || *ptr == '\t')) {
        info->has_leading_ws = true;
    }

    if (len >= 32) {
        __m256i equals_vec = _mm256_set1_epi8('=');
        __m256i colon_vec  = _mm256_set1_epi8(':');
        __m256i hash_vec   = _mm256_set1_epi8('#');

        while (ptr + 32 <= str_end) {
            __m256i data      = _mm256_loadu_si256((const __m256i*)ptr);
            __m256i cmp_eq    = _mm256_cmpeq_epi8(data, equals_vec);
            __m256i cmp_colon = _mm256_cmpeq_epi8(data, colon_vec);
            __m256i cmp_hash  = _mm256_cmpeq_epi8(data, hash_vec);

            int mask_eq    = _mm256_movemask_epi8(cmp_eq);
            int mask_colon = _mm256_movemask_epi8(cmp_colon);
            int mask_hash  = _mm256_movemask_epi8(cmp_hash);

            // Check for comment first (highest priority)
            if (mask_hash != 0) {
                int offset     = __builtin_ctz(mask_hash);
                info->sep_char = '#';
                info->sep_pos  = ptr + offset;
                return;
            }

            // Find which separator comes first
            if (mask_eq != 0 && mask_colon != 0) {
                int offset_eq    = __builtin_ctz(mask_eq);
                int offset_colon = __builtin_ctz(mask_colon);
                if (offset_eq < offset_colon) {
                    info->sep_char = '=';
                    info->sep_pos  = ptr + offset_eq;
                } else {
                    info->sep_char = ':';
                    info->sep_pos  = ptr + offset_colon;
                }
                return;
            } else if (mask_eq != 0) {
                int offset     = __builtin_ctz(mask_eq);
                info->sep_char = '=';
                info->sep_pos  = ptr + offset;
                return;
            } else if (mask_colon != 0) {
                int offset     = __builtin_ctz(mask_colon);
                info->sep_char = ':';
                info->sep_pos  = ptr + offset;
                return;
            }

            ptr += 32;
        }
    }

    // Scalar fallback for remaining bytes
    while (ptr < str_end) {
        if (*ptr == '#') {
            info->sep_char = '#';
            info->sep_pos  = ptr;
            return;
        }
        if (*ptr == '=') {
            info->sep_char = '=';
            info->sep_pos  = ptr;
            return;
        }
        if (*ptr == ':') {
            info->sep_char = ':';
            info->sep_pos  = ptr;
            return;
        }
        ptr++;
    }
}

/* ============================================================================
 * Main parsing function with all 4 optimizations
 * ============================================================================ */

void infofile_parse_string(const char* data, size_t len, InfoFile* info) {
    const char* ptr = data;
    const char* end = data + len;

    const char* current_key       = NULL;
    char*       value_buffer      = NULL;
    size_t      value_buffer_size = 0;
    size_t      value_buffer_used = 0;
    bool        is_multiline      = false;

    while (ptr < end) {
        // OPTIMIZATION 1: Zero-copy - find newline without copying line
        const char* line_start  = ptr;
        const char* newline_pos = simd_find_newline_opt(ptr, end);

        const char* line_end;
        if (newline_pos) {
            line_end = newline_pos;
            ptr      = newline_pos + 1;
        } else {
            line_end = end;
            ptr      = end;
        }

        // Remove \r if present (Windows line endings)
        if (line_end > line_start && *(line_end - 1) == '\r') {
            line_end--;
        }

        size_t line_len = line_end - line_start;
        if (line_len == 0)
            continue; // Skip empty lines

        // OPTIMIZATION 2 & 3: SIMD character class detection
        // Check for empty lines, comments, and find separators in one pass
        const char *trim_start, *trim_end;
        simd_trim_zero_copy(line_start, line_end, &trim_start, &trim_end);

        // Skip empty lines or comments
        if (trim_start >= trim_end || *trim_start == '#') {
            continue;
        }

        // Check if this is a continuation line (starts with whitespace)
        bool is_continuation = (line_start < line_end &&
                                (*line_start == '\t' || (*line_start == ' ' && line_len > 1)));

        if (is_continuation) {
            if (is_multiline) {
                // Append to multiline value buffer
                size_t trimmed_len = trim_end - trim_start;
                if (trimmed_len > 0) {
                    size_t needed = trimmed_len + 2;

                    if (value_buffer_used + needed > value_buffer_size) {
                        size_t new_size = value_buffer_size ? value_buffer_size * 2 : 1024;
                        while (new_size < value_buffer_used + needed)
                            new_size *= 2;
                        char* new_buffer = realloc(value_buffer, new_size);
                        if (!new_buffer) {
                            fprintf(stderr, "FATAL: Failed to reallocate value buffer (%zu bytes)\n", new_size);
                            free(value_buffer);
                            exit(1);
                        }
                        value_buffer      = new_buffer;
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
                info->entries[info->count].value =
                    arena_strndup(&info->arena.value_arena, value_buffer, value_buffer_used);
            } else {
                info->entries[info->count].value =
                    arena_strndup(&info->arena.value_arena, "", 0);
            }
            info->count++;
            current_key = NULL;
            if (value_buffer) {
                value_buffer_used = 0;
                value_buffer[0]   = '\0';
            }
            is_multiline = false;
        }

        // OPTIMIZATION 3: Find separator with SIMD
        SeparatorInfo sep_info;
        simd_find_special_chars(trim_start, trim_end, &sep_info);

        if (sep_info.sep_char == '\0' || sep_info.sep_char == '#') {
            continue;
        }

        if (sep_info.sep_char == '=') {
            // Single-line format: key = value
            // OPTIMIZATION 2: SIMD trim both key and value
            const char *key_start, *key_end;
            simd_trim_zero_copy(trim_start, sep_info.sep_pos, &key_start, &key_end);

            const char *val_start, *val_end;
            simd_trim_zero_copy(sep_info.sep_pos + 1, trim_end, &val_start, &val_end);

            // OPTIMIZATION 4: Store key in key_arena, value in value_arena
            size_t key_len = key_end - key_start;
            size_t val_len = val_end - val_start;

            current_key       = arena_strndup(&info->arena.key_arena, key_start, key_len);
            const char* value = arena_strndup(&info->arena.value_arena, val_start, val_len);

            ensure_capacity(info);
            info->entries[info->count].key   = current_key;
            info->entries[info->count].value = value;
            info->count++;
            current_key = NULL;
        } else if (sep_info.sep_char == ':') {
            // Multi-line format: key:
            const char *key_start, *key_end;
            simd_trim_zero_copy(trim_start, sep_info.sep_pos, &key_start, &key_end);

            size_t key_len = key_end - key_start;
            current_key    = arena_strndup(&info->arena.key_arena, key_start, key_len);

            // Check if there's content after the colon
            if (sep_info.sep_pos + 1 < trim_end) {
                const char *after_start, *after_end;
                simd_trim_zero_copy(sep_info.sep_pos + 1, trim_end, &after_start, &after_end);

                size_t content_len = after_end - after_start;
                if (content_len > 0) {
                    if (!value_buffer) {
                        value_buffer_size = 1024;
                        value_buffer      = malloc(value_buffer_size);
                        if (!value_buffer) {
                            fprintf(stderr, "FATAL: Failed to allocate value buffer (%zu bytes)\n", value_buffer_size);
                            exit(1);
                        }
                    }
                    if (content_len + 1 > value_buffer_size) {
                        value_buffer_size = content_len + 1024;
                        char* new_buffer  = realloc(value_buffer, value_buffer_size);
                        if (!new_buffer) {
                            fprintf(stderr, "FATAL: Failed to reallocate value buffer (%zu bytes)\n", value_buffer_size);
                            free(value_buffer);
                            exit(1);
                        }
                        value_buffer = new_buffer;
                    }
                    memcpy(value_buffer, after_start, content_len);
                    value_buffer[content_len] = '\0';
                    value_buffer_used         = content_len;
                } else {
                    value_buffer_used = 0;
                    if (value_buffer)
                        value_buffer[0] = '\0';
                }
            } else {
                value_buffer_used = 0;
                if (value_buffer)
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
            info->entries[info->count].value =
                arena_strndup(&info->arena.value_arena, value_buffer, value_buffer_used);
        } else {
            info->entries[info->count].value =
                arena_strndup(&info->arena.value_arena, "", 0);
        }
        info->count++;
    }

    if (value_buffer) {
        free(value_buffer);
    }
}

void infofile_parse_file(const char* filename, InfoFile* info) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "FATAL: Failed to open file '%s'\n", filename);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Pre-allocate entries array based on file size
    size_t estimated_entries = file_size / 150;
    if (estimated_entries > info->capacity) {
        size_t new_capacity = estimated_entries;
        InfoFileEntry* new_entries = realloc(info->entries, new_capacity * sizeof(InfoFileEntry));
        if (!new_entries) {
            fprintf(stderr, "FATAL: Failed to reallocate entries array (%zu bytes)\n",
                    new_capacity * sizeof(InfoFileEntry));
            fclose(fp);
            exit(1);
        }
        info->entries = new_entries;
        info->capacity = new_capacity;
    }

    // Pre-allocate both arenas to avoid reallocation during parsing
    // Keys typically use less space than values
    size_t estimated_key_size   = file_size / 3;     // ~33% for keys
    size_t estimated_value_size = file_size * 5 / 3; // ~166% for values (with overhead)

    arena_reserve(&info->arena.key_arena, estimated_key_size);
    arena_reserve(&info->arena.value_arena, estimated_value_size);

    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "FATAL: Failed to allocate file buffer (%ld bytes)\n", file_size + 1);
        fclose(fp);
        exit(1);
    }

    size_t bytes_read  = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    infofile_parse_string(buffer, bytes_read, info);
    free(buffer);
}

const char* infofile_get(const InfoFile* info, const char* key) {
    // Linear search - keys are in key_arena for better cache locality
    for (size_t i = 0; i < info->count; i++) {
        if (strcmp(info->entries[i].key, key) == 0) {
            return info->entries[i].value;
        }
    }
    return NULL;
}

void infofile_free(InfoFile* info) {
    free(info->entries);
    arena_free(&info->arena.key_arena);
    arena_free(&info->arena.value_arena);
    info->entries  = NULL;
    info->count    = 0;
    info->capacity = 0;
}
