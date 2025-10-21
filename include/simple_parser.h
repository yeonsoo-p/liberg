#ifndef SIMPLE_PARSER_H
#define SIMPLE_PARSER_H

#include <stdlib.h>

/* Property structure for key-value pairs */
typedef struct Property {
    char *key;
    char *value;
    struct Property *next;
} Property;

/* Data section structure for multi-line data */
typedef struct DataSection {
    char *key;
    char **lines;
    int line_count;
    int capacity;
    struct DataSection *next;
} DataSection;

/* Main info file structure */
typedef struct InfoFile {
    Property *properties;
    DataSection *data_sections;
    int property_count;
    int data_section_count;
} InfoFile;

/* Parser functions */
InfoFile *parse_info_file(const char *filename);
void free_info_file(InfoFile *file);

/* Query functions */
const char *get_property(InfoFile *file, const char *key);
DataSection *get_data_section(InfoFile *file, const char *key);

/* Utility functions */
void print_info_file(InfoFile *file);

#endif /* SIMPLE_PARSER_H */
