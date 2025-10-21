#ifndef INFOFILE_ARENA_SIMD_H
#define INFOFILE_ARENA_SIMD_H

#include <stddef.h>
#include <arena.h>

/**
 * Represents a key-value pair from an info file (arena+SIMD version)
 */
typedef struct {
    const char *key;    /* The key (points into arena) */
    const char *value;  /* The value (points into arena) */
} InfoFileEntryArenaSimd;

/**
 * Represents a parsed info file (arena+SIMD optimized version)
 */
typedef struct {
    InfoFileEntryArenaSimd *entries;  /* Array of entries */
    size_t count;                     /* Number of entries */
    size_t capacity;                  /* Allocated capacity */
    Arena arena;                      /* String storage arena */
} InfoFileArenaSimd;

/**
 * Initialize an InfoFileArenaSimd structure
 */
void infofile_arena_simd_init(InfoFileArenaSimd *info);

/**
 * Parse an info file from a file path (arena+SIMD optimized)
 * Returns 0 on success, -1 on error
 */
int infofile_arena_simd_parse_file(const char *filename, InfoFileArenaSimd *info);

/**
 * Parse an info file from a string buffer (arena+SIMD optimized)
 * Returns 0 on success, -1 on error
 */
int infofile_arena_simd_parse_string(const char *data, size_t len, InfoFileArenaSimd *info);

/**
 * Get a value by key (returns NULL if not found)
 */
const char *infofile_arena_simd_get(const InfoFileArenaSimd *info, const char *key);

/**
 * Free all memory associated with an InfoFileArenaSimd structure
 */
void infofile_arena_simd_free(InfoFileArenaSimd *info);

#endif /* INFOFILE_ARENA_SIMD_H */
