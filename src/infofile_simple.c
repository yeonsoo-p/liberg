#include "infofile_simple.h"
#include "simple_parser.h"
#include <string.h>
#include <stdio.h>

// Helper structure to cache multiline values as single strings
typedef struct CachedValue {
    char *key;
    char *value;
    struct CachedValue *next;
} CachedValue;

typedef struct {
    InfoFile *file;
    CachedValue *cached_multiline;
} SimpleWrapper;

void infofile_simple_init(InfoFileSimple *info) {
    info->internal = NULL;
    info->count = 0;
}

int infofile_simple_parse_file(const char *filename, InfoFileSimple *info) {
    InfoFile *file = parse_info_file(filename);
    if (!file) {
        return -1;
    }

    SimpleWrapper *wrapper = (SimpleWrapper *)malloc(sizeof(SimpleWrapper));
    wrapper->file = file;
    wrapper->cached_multiline = NULL;

    info->internal = wrapper;
    info->count = file->property_count + file->data_section_count;

    return 0;
}

const char *infofile_simple_get(InfoFileSimple *info, const char *key) {
    if (!info || !info->internal || !key) {
        return NULL;
    }

    SimpleWrapper *wrapper = (SimpleWrapper *)info->internal;
    InfoFile *file = wrapper->file;

    // First try to find it as a single-line property
    const char *prop_value = get_property(file, key);
    if (prop_value) {
        return prop_value;
    }

    // Check if we've already cached this multiline value
    CachedValue *cached = wrapper->cached_multiline;
    while (cached) {
        if (strcmp(cached->key, key) == 0) {
            return cached->value;
        }
        cached = cached->next;
    }

    // Try to find it as a data section
    DataSection *section = get_data_section(file, key);
    if (!section) {
        return NULL;
    }

    // Convert data section lines to single string with newlines
    // Calculate total size needed
    size_t total_size = 0;
    for (int i = 0; i < section->line_count; i++) {
        total_size += strlen(section->lines[i]);
        if (i < section->line_count - 1) {
            total_size += 1;  // for newline
        }
    }
    total_size += 1;  // for null terminator

    // Allocate and build the string
    char *value = (char *)malloc(total_size);
    char *ptr = value;
    for (int i = 0; i < section->line_count; i++) {
        size_t len = strlen(section->lines[i]);
        memcpy(ptr, section->lines[i], len);
        ptr += len;
        if (i < section->line_count - 1) {
            *ptr++ = '\n';
        }
    }
    *ptr = '\0';

    // Cache this value
    CachedValue *new_cached = (CachedValue *)malloc(sizeof(CachedValue));
    new_cached->key = strdup(key);
    new_cached->value = value;
    new_cached->next = wrapper->cached_multiline;
    wrapper->cached_multiline = new_cached;

    return value;
}

void infofile_simple_free(InfoFileSimple *info) {
    if (!info || !info->internal) {
        return;
    }

    SimpleWrapper *wrapper = (SimpleWrapper *)info->internal;

    // Free cached multiline values
    CachedValue *cached = wrapper->cached_multiline;
    while (cached) {
        CachedValue *next = cached->next;
        free(cached->key);
        free(cached->value);
        free(cached);
        cached = next;
    }

    // Free the original file structure
    free_info_file(wrapper->file);

    free(wrapper);
    info->internal = NULL;
    info->count = 0;
}
