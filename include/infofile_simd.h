#ifndef INFOFILE_SIMD_H
#define INFOFILE_SIMD_H

#include <stddef.h>

/**
 * Represents a key-value pair from an info file (SIMD version)
 * Uses same structure as baseline for compatibility
 */
typedef struct {
    char *key;      /* The key (allocated) */
    char *value;    /* The value (allocated, may contain newlines for multiline) */
} InfoFileEntrySIMD;

/**
 * Represents a parsed info file (SIMD version)
 */
typedef struct {
    InfoFileEntrySIMD *entries;  /* Array of entries */
    size_t count;                /* Number of entries */
    size_t capacity;             /* Allocated capacity */
} InfoFileSIMD;

/**
 * Initialize an InfoFileSIMD structure
 */
void infofile_simd_init(InfoFileSIMD *info);

/**
 * Parse an info file from a file path (SIMD optimized)
 * Returns 0 on success, -1 on error
 */
int infofile_simd_parse_file(const char *filename, InfoFileSIMD *info);

/**
 * Parse an info file from a string buffer (SIMD optimized)
 * Returns 0 on success, -1 on error
 */
int infofile_simd_parse_string(const char *data, size_t len, InfoFileSIMD *info);

/**
 * Get a value by key (returns NULL if not found)
 */
const char *infofile_simd_get(const InfoFileSIMD *info, const char *key);

/**
 * Set or update a key-value pair
 * Returns 0 on success, -1 on error
 */
int infofile_simd_set(InfoFileSIMD *info, const char *key, const char *value);

/**
 * Write an InfoFileSIMD structure to a file
 * Returns 0 on success, -1 on error
 */
int infofile_simd_write_file(const char *filename, const InfoFileSIMD *info);

/**
 * Write an InfoFileSIMD structure to a string buffer (allocates memory)
 * Returns allocated string on success, NULL on error
 * Caller must free the returned string
 */
char *infofile_simd_write_string(const InfoFileSIMD *info);

/**
 * Free all memory associated with an InfoFileSIMD structure
 */
void infofile_simd_free(InfoFileSIMD *info);

#endif /* INFOFILE_SIMD_H */
