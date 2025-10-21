#ifndef INFOFILE_ARENA_H
#define INFOFILE_ARENA_H

#include <stddef.h>
#include <arena.h>

/**
 * Represents a key-value pair from an info file (arena version)
 */
typedef struct {
    const char *key;    /* The key (points into arena) */
    const char *value;  /* The value (points into arena) */
} InfoFileEntryArena;

/**
 * Represents a parsed info file (arena version)
 */
typedef struct {
    InfoFileEntryArena *entries;  /* Array of entries */
    size_t count;                 /* Number of entries */
    size_t capacity;              /* Allocated capacity */
    Arena arena;                  /* String storage arena */
} InfoFileArena;

/**
 * Initialize an InfoFileArena structure
 */
void infofile_arena_init(InfoFileArena *info);

/**
 * Parse an info file from a file path (arena version)
 * Returns 0 on success, -1 on error
 */
int infofile_arena_parse_file(const char *filename, InfoFileArena *info);

/**
 * Parse an info file from a string buffer (arena version)
 * Returns 0 on success, -1 on error
 */
int infofile_arena_parse_string(const char *data, size_t len, InfoFileArena *info);

/**
 * Get a value by key (returns NULL if not found)
 */
const char *infofile_arena_get(const InfoFileArena *info, const char *key);

/**
 * Free all memory associated with an InfoFileArena structure
 */
void infofile_arena_free(InfoFileArena *info);

#endif /* INFOFILE_ARENA_H */
