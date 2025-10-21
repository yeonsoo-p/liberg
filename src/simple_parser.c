#include "simple_parser.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Helper: trim whitespace from both ends */
static char *trim(char *str) {
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

/* Helper: create property */
static Property *create_property(const char *key, const char *value) {
    Property *prop = (Property *)malloc(sizeof(Property));
    prop->key = strdup(key);
    prop->value = strdup(value);
    prop->next = NULL;
    return prop;
}

/* Helper: add property to list */
static void add_property(InfoFile *file, Property *prop) {
    if (!file->properties) {
        file->properties = prop;
    } else {
        Property *curr = file->properties;
        while (curr->next) curr = curr->next;
        curr->next = prop;
    }
    file->property_count++;
}

/* Helper: create data section */
static DataSection *create_data_section(const char *key) {
    DataSection *section = (DataSection *)malloc(sizeof(DataSection));
    section->key = strdup(key);
    section->lines = NULL;
    section->line_count = 0;
    section->capacity = 0;
    section->next = NULL;
    return section;
}

/* Helper: add line to data section */
static void add_data_line(DataSection *section, const char *line) {
    if (section->line_count >= section->capacity) {
        section->capacity = section->capacity == 0 ? 10 : section->capacity * 2;
        section->lines = (char **)realloc(section->lines, section->capacity * sizeof(char *));
    }
    section->lines[section->line_count++] = strdup(line);
}

/* Helper: add data section to list */
static void add_data_section(InfoFile *file, DataSection *section) {
    if (!file->data_sections) {
        file->data_sections = section;
    } else {
        DataSection *curr = file->data_sections;
        while (curr->next) curr = curr->next;
        curr->next = section;
    }
    file->data_section_count++;
}

/* Main parser function */
InfoFile *parse_info_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return NULL;
    }

    InfoFile *file = (InfoFile *)calloc(1, sizeof(InfoFile));
    char line[8192];
    DataSection *current_section = NULL;

    /* Keep tail pointers to avoid O(n) list traversal */
    Property *prop_tail = NULL;
    DataSection *section_tail = NULL;

    while (fgets(line, sizeof(line), fp)) {
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        if (len > 0 && line[len-1] == '\r') {
            line[len-1] = '\0';
            len--;
        }

        /* If we're in a data section */
        if (current_section) {
            /* Check if line starts with tab (data continuation) */
            if (line[0] == '\t') {
                add_data_line(current_section, trim(line + 1));
                continue;
            } else {
                /* End of data section */
                current_section = NULL;
                /* Fall through to parse this line normally */
            }
        }

        /* Skip empty lines */
        char *trimmed = trim(line);
        if (trimmed[0] == '\0') continue;

        /* Skip comments */
        if (trimmed[0] == '#') continue;

        /* Check for key:value (multi-line data section) */
        char *colon = strchr(trimmed, ':');
        if (colon && *(colon + 1) == '\0') {
            /* This is a data section start */
            *colon = '\0';
            char *key = trim(trimmed);
            current_section = create_data_section(key);
            /* O(1) append using tail pointer */
            if (!section_tail) {
                file->data_sections = current_section;
            } else {
                section_tail->next = current_section;
            }
            section_tail = current_section;
            file->data_section_count++;
            continue;
        }

        /* Check for key=value (single-line property) */
        char *equals = strchr(trimmed, '=');
        if (equals) {
            *equals = '\0';
            char *key = trim(trimmed);
            char *value = trim(equals + 1);

            Property *prop = create_property(key, value);
            /* O(1) append using tail pointer */
            if (!prop_tail) {
                file->properties = prop;
            } else {
                prop_tail->next = prop;
            }
            prop_tail = prop;
            file->property_count++;
            continue;
        }

        /* Unrecognized line - skip */
        fprintf(stderr, "Warning: skipping unrecognized line: %s\n", trimmed);
    }

    fclose(fp);
    return file;
}

/* Free the entire structure */
void free_info_file(InfoFile *file) {
    if (!file) return;

    /* Free properties */
    Property *prop = file->properties;
    while (prop) {
        Property *next = prop->next;
        free(prop->key);
        free(prop->value);
        free(prop);
        prop = next;
    }

    /* Free data sections */
    DataSection *section = file->data_sections;
    while (section) {
        DataSection *next = section->next;
        free(section->key);
        for (int i = 0; i < section->line_count; i++) {
            free(section->lines[i]);
        }
        free(section->lines);
        free(section);
        section = next;
    }

    free(file);
}

/* Get property by key */
const char *get_property(InfoFile *file, const char *key) {
    if (!file || !key) return NULL;

    Property *prop = file->properties;
    while (prop) {
        if (strcmp(prop->key, key) == 0) {
            return prop->value;
        }
        prop = prop->next;
    }
    return NULL;
}

/* Get data section by key */
DataSection *get_data_section(InfoFile *file, const char *key) {
    if (!file || !key) return NULL;

    DataSection *section = file->data_sections;
    while (section) {
        if (strcmp(section->key, key) == 0) {
            return section;
        }
        section = section->next;
    }
    return NULL;
}

/* Print entire file */
void print_info_file(InfoFile *file) {
    if (!file) return;

    printf("=== Properties (%d) ===\n", file->property_count);
    Property *prop = file->properties;
    while (prop) {
        printf("%s = %s\n", prop->key, prop->value);
        prop = prop->next;
    }

    printf("\n=== Data Sections (%d) ===\n", file->data_section_count);
    DataSection *section = file->data_sections;
    while (section) {
        printf("%s: (%d lines)\n", section->key, section->line_count);
        for (int i = 0; i < section->line_count && i < 3; i++) {
            printf("  %s\n", section->lines[i]);
        }
        if (section->line_count > 3) {
            printf("  ... (%d more lines)\n", section->line_count - 3);
        }
        section = section->next;
    }
}
