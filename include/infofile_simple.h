#ifndef INFOFILE_SIMPLE_H
#define INFOFILE_SIMPLE_H

#include <stdlib.h>

// Wrapper around simple_parser to provide unified API
typedef struct {
    void *internal;  // Points to InfoFile from simple_parser
    size_t count;
} InfoFileSimple;

void infofile_simple_init(InfoFileSimple *info);
int infofile_simple_parse_file(const char *filename, InfoFileSimple *info);
const char *infofile_simple_get(InfoFileSimple *info, const char *key);
void infofile_simple_free(InfoFileSimple *info);

#endif /* INFOFILE_SIMPLE_H */
