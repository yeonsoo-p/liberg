#ifndef ERGINFO_H
#define ERGINFO_H

#include <stddef.h>

/**
 * Represents a key-value pair from an erg.info file
 */
typedef struct {
    char *key;      /* The key (allocated) */
    char *value;    /* The value (allocated, may contain newlines for multiline) */
} ErgInfoEntry;

/**
 * Represents a parsed erg.info file
 */
typedef struct {
    ErgInfoEntry *entries;  /* Array of entries */
    size_t count;          /* Number of entries */
    size_t capacity;       /* Allocated capacity */
} ErgInfo;

/**
 * Initialize an ErgInfo structure
 */
void erginfo_init(ErgInfo *info);

/**
 * Parse an erg.info file from a file path
 * Returns 0 on success, -1 on error
 */
int erginfo_parse_file(const char *filename, ErgInfo *info);

/**
 * Parse an erg.info file from a string buffer
 * Returns 0 on success, -1 on error
 */
int erginfo_parse_string(const char *data, size_t len, ErgInfo *info);

/**
 * Get a value by key (returns NULL if not found)
 */
const char *erginfo_get(const ErgInfo *info, const char *key);

/**
 * Set or update a key-value pair
 * Returns 0 on success, -1 on error
 */
int erginfo_set(ErgInfo *info, const char *key, const char *value);

/**
 * Write an ErgInfo structure to a file
 * Returns 0 on success, -1 on error
 */
int erginfo_write_file(const char *filename, const ErgInfo *info);

/**
 * Write an ErgInfo structure to a string buffer (allocates memory)
 * Returns allocated string on success, NULL on error
 * Caller must free the returned string
 */
char *erginfo_write_string(const ErgInfo *info);

/**
 * Free all memory associated with an ErgInfo structure
 */
void erginfo_free(ErgInfo *info);

#endif /* ERGINFO_H */
