#ifndef INFOFILE_H
#define INFOFILE_H

#include <stddef.h>

/**
 * Represents a key-value pair from an info file
 */
typedef struct {
    char *key;      /* The key (allocated) */
    char *value;    /* The value (allocated, may contain newlines for multiline) */
} InfoFileEntry;

/**
 * Represents a parsed info file
 */
typedef struct {
    InfoFileEntry *entries;  /* Array of entries */
    size_t count;           /* Number of entries */
    size_t capacity;        /* Allocated capacity */
} InfoFile;

/**
 * Initialize an InfoFile structure
 */
void infofile_init(InfoFile *info);

/**
 * Parse an info file from a file path
 * Returns 0 on success, -1 on error
 */
int infofile_parse_file(const char *filename, InfoFile *info);

/**
 * Parse an info file from a string buffer
 * Returns 0 on success, -1 on error
 */
int infofile_parse_string(const char *data, size_t len, InfoFile *info);

/**
 * Get a value by key (returns NULL if not found)
 */
const char *infofile_get(const InfoFile *info, const char *key);

/**
 * Set or update a key-value pair
 * Returns 0 on success, -1 on error
 */
int infofile_set(InfoFile *info, const char *key, const char *value);

/**
 * Write an InfoFile structure to a file
 * Returns 0 on success, -1 on error
 */
int infofile_write_file(const char *filename, const InfoFile *info);

/**
 * Write an InfoFile structure to a string buffer (allocates memory)
 * Returns allocated string on success, NULL on error
 * Caller must free the returned string
 */
char *infofile_write_string(const InfoFile *info);

/**
 * Free all memory associated with an InfoFile structure
 */
void infofile_free(InfoFile *info);

#endif /* INFOFILE_H */
