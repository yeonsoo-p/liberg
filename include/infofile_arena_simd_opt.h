#ifndef INFOFILE_ARENA_SIMD_OPT_H
#define INFOFILE_ARENA_SIMD_OPT_H

#include <stddef.h>
#include <arena.h>

/**
 * Dual arena allocator for optimized cache locality
 * Keys (hot) and values (cold) stored separately
 */
typedef struct {
    Arena key_arena;    /* Hot data - accessed during lookups */
    Arena value_arena;  /* Cold data - accessed only when found */
} DualArena;

/**
 * Represents a key-value pair (optimized version)
 */
typedef struct {
    const char *key;    /* Points into key_arena */
    const char *value;  /* Points into value_arena */
} InfoFileEntryArenaSimdOpt;

/**
 * Represents a parsed info file (fully optimized version)
 * Uses zero-copy parsing, SIMD whitespace trimming,
 * SIMD character class detection, and dual-arena layout
 */
typedef struct {
    InfoFileEntryArenaSimdOpt *entries;  /* Array of entries */
    size_t count;                        /* Number of entries */
    size_t capacity;                     /* Allocated capacity */
    DualArena arena;                     /* Dual arena for keys/values */
} InfoFileArenaSimdOpt;

/**
 * Initialize an InfoFileArenaSimdOpt structure
 */
void infofile_arena_simd_opt_init(InfoFileArenaSimdOpt *info);

/**
 * Parse an info file from a file path (fully optimized)
 * Returns 0 on success, -1 on error
 */
int infofile_arena_simd_opt_parse_file(const char *filename, InfoFileArenaSimdOpt *info);

/**
 * Parse an info file from a string buffer (fully optimized)
 * Returns 0 on success, -1 on error
 */
int infofile_arena_simd_opt_parse_string(const char *data, size_t len, InfoFileArenaSimdOpt *info);

/**
 * Get a value by key (returns NULL if not found)
 */
const char *infofile_arena_simd_opt_get(const InfoFileArenaSimdOpt *info, const char *key);

/**
 * Free all memory associated with an InfoFileArenaSimdOpt structure
 */
void infofile_arena_simd_opt_free(InfoFileArenaSimdOpt *info);

#endif /* INFOFILE_ARENA_SIMD_OPT_H */
