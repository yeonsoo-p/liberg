#include <erg.h>
#include <infofile.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERG_HEADER_SIZE 16

/* Helper function to parse data type string to ERGDataType enum */
static ERGDataType parse_data_type(const char* type_str, size_t* type_size) {
    if (strcmp(type_str, "Float") == 0) {
        *type_size = 4;
        return ERG_FLOAT;
    } else if (strcmp(type_str, "Double") == 0) {
        *type_size = 8;
        return ERG_DOUBLE;
    } else if (strcmp(type_str, "LongLong") == 0) {
        *type_size = 8;
        return ERG_LONGLONG;
    } else if (strcmp(type_str, "ULongLong") == 0) {
        *type_size = 8;
        return ERG_ULONGLONG;
    } else if (strcmp(type_str, "Int") == 0) {
        *type_size = 4;
        return ERG_INT;
    } else if (strcmp(type_str, "UInt") == 0) {
        *type_size = 4;
        return ERG_UINT;
    } else if (strcmp(type_str, "Short") == 0) {
        *type_size = 2;
        return ERG_SHORT;
    } else if (strcmp(type_str, "UShort") == 0) {
        *type_size = 2;
        return ERG_USHORT;
    } else if (strcmp(type_str, "Char") == 0) {
        *type_size = 1;
        return ERG_CHAR;
    } else if (strcmp(type_str, "UChar") == 0) {
        *type_size = 1;
        return ERG_UCHAR;
    } else if (strstr(type_str, "Bytes") != NULL) {
        /* Parse "N Bytes" format (e.g., "8 Bytes") */
        int n = atoi(type_str);
        if (n >= 1 && n <= 8) {
            *type_size = n;
            return ERG_BYTES;
        }
    }

    *type_size = 0;
    return ERG_UNKNOWN;
}

/* Helper function to convert string to double */
static double parse_double(const char* str) {
    if (!str)
        return 0.0;
    return atof(str);
}

/* Byte swap functions for endianness conversion */
static uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static uint32_t swap_uint32(uint32_t val) {
    return ((val << 24) & 0xFF000000) |
           ((val << 8) & 0x00FF0000) |
           ((val >> 8) & 0x0000FF00) |
           ((val >> 24) & 0x000000FF);
}

static uint64_t swap_uint64(uint64_t val) {
    return ((val << 56) & 0xFF00000000000000ULL) |
           ((val << 40) & 0x00FF000000000000ULL) |
           ((val << 24) & 0x0000FF0000000000ULL) |
           ((val << 8) & 0x000000FF00000000ULL) |
           ((val >> 8) & 0x00000000FF000000ULL) |
           ((val >> 24) & 0x0000000000FF0000ULL) |
           ((val >> 40) & 0x000000000000FF00ULL) |
           ((val >> 56) & 0x00000000000000FFULL);
}

/* Check if system is little-endian */
static int is_little_endian_system(void) {
    uint16_t test = 1;
    return *((uint8_t*)&test) == 1;
}

void erg_init(ERG* erg, const char* erg_file_path) {
    memset(erg, 0, sizeof(ERG));

    /* Store ERG file path */
    size_t path_len = strlen(erg_file_path);
    erg->erg_path   = malloc(path_len + 1);
    if (!erg->erg_path) {
        fprintf(stderr, "FATAL: Failed to allocate ERG path (%zu bytes)\n", path_len + 1);
        exit(1);
    }
    strcpy(erg->erg_path, erg_file_path);

    /* Initialize info file structure */
    erg->info = malloc(sizeof(InfoFile));
    if (!erg->info) {
        fprintf(stderr, "FATAL: Failed to allocate InfoFile structure\n");
        exit(1);
    }
}

void erg_parse(ERG* erg) {
    /* Build info file path (.erg.info) */
    size_t info_path_len = strlen(erg->erg_path) + 6; /* +".info" */
    char*  info_path     = malloc(info_path_len);
    if (!info_path) {
        fprintf(stderr, "FATAL: Failed to allocate info path buffer\n");
        exit(1);
    }
    snprintf(info_path, info_path_len, "%s.info", erg->erg_path);

    /* Parse info file */
    infofile_init(erg->info);
    infofile_parse_file(info_path, erg->info);
    free(info_path);

    /* Get byte order */
    const char* byte_order = infofile_get(erg->info, "File.ByteOrder");
    if (!byte_order) {
        fprintf(stderr, "FATAL: File.ByteOrder not found in ERG info file\n");
        exit(1);
    }
    erg->little_endian = (strcmp(byte_order, "LittleEndian") == 0);

    /* Parse signal metadata */
    /* Count signals first */
    size_t signal_count = 0;
    char   key_buffer[256];
    while (1) {
        snprintf(key_buffer, sizeof(key_buffer), "File.At.%zu.Name", signal_count + 1);
        const char* name = infofile_get(erg->info, key_buffer);
        if (!name)
            break;
        signal_count++;
    }

    if (signal_count == 0) {
        fprintf(stderr, "FATAL: No signals found in ERG info file\n");
        exit(1);
    }

    /* Allocate signal array */
    erg->signal_count = signal_count;
    erg->signals      = calloc(signal_count, sizeof(ERGSignal));
    if (!erg->signals) {
        fprintf(stderr, "FATAL: Failed to allocate signals array (%zu bytes)\n",
                signal_count * sizeof(ERGSignal));
        exit(1);
    }

    /* Parse each signal's metadata */
    erg->row_size = 0;
    for (size_t i = 0; i < signal_count; i++) {
        ERGSignal* sig = &erg->signals[i];

        /* Get signal name */
        snprintf(key_buffer, sizeof(key_buffer), "File.At.%zu.Name", i + 1);
        const char* name = infofile_get(erg->info, key_buffer);
        sig->name        = malloc(strlen(name) + 1);
        if (!sig->name) {
            fprintf(stderr, "FATAL: Failed to allocate signal name\n");
            exit(1);
        }
        strcpy(sig->name, name);

        /* Get data type */
        snprintf(key_buffer, sizeof(key_buffer), "File.At.%zu.Type", i + 1);
        const char* type_str = infofile_get(erg->info, key_buffer);
        if (!type_str) {
            fprintf(stderr, "FATAL: Data type not found for signal %s\n", name);
            exit(1);
        }
        sig->type = parse_data_type(type_str, &sig->type_size);

        /* Get unit */
        snprintf(key_buffer, sizeof(key_buffer), "Quantity.%s.Unit", name);
        const char* unit = infofile_get(erg->info, key_buffer);
        if (unit) {
            sig->unit = malloc(strlen(unit) + 1);
            if (sig->unit) {
                strcpy(sig->unit, unit);
            }
        } else {
            sig->unit = malloc(1);
            if (sig->unit) {
                sig->unit[0] = '\0';
            }
        }

        /* Get scaling factor */
        snprintf(key_buffer, sizeof(key_buffer), "Quantity.%s.Factor", name);
        const char* factor_str = infofile_get(erg->info, key_buffer);
        sig->factor            = factor_str ? parse_double(factor_str) : 1.0;

        /* Get scaling offset */
        snprintf(key_buffer, sizeof(key_buffer), "Quantity.%s.Offset", name);
        const char* offset_str = infofile_get(erg->info, key_buffer);
        sig->offset            = offset_str ? parse_double(offset_str) : 0.0;

        /* Accumulate row size */
        erg->row_size += sig->type_size;
    }

    /* Read binary ERG file */
    FILE* fp = fopen(erg->erg_path, "rb");
    if (!fp) {
        fprintf(stderr, "FATAL: Failed to open ERG file '%s'\n", erg->erg_path);
        exit(1);
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= ERG_HEADER_SIZE) {
        fprintf(stderr, "FATAL: ERG file too small (%ld bytes)\n", file_size);
        fclose(fp);
        exit(1);
    }

    /* Skip header */
    fseek(fp, ERG_HEADER_SIZE, SEEK_SET);

    /* Calculate data size and sample count */
    erg->data_size    = file_size - ERG_HEADER_SIZE;
    erg->sample_count = erg->data_size / erg->row_size;

    /* Allocate buffer for raw data */
    erg->raw_data = malloc(erg->data_size);
    if (!erg->raw_data) {
        fprintf(stderr, "FATAL: Failed to allocate data buffer (%zu bytes)\n", erg->data_size);
        fclose(fp);
        exit(1);
    }

    /* Read all data */
    size_t bytes_read = fread(erg->raw_data, 1, erg->data_size, fp);
    if (bytes_read != erg->data_size) {
        fprintf(stderr, "FATAL: Failed to read ERG data (expected %zu, got %zu bytes)\n",
                erg->data_size, bytes_read);
        fclose(fp);
        exit(1);
    }

    fclose(fp);

    /* Handle endianness conversion if needed */
    int system_little_endian = is_little_endian_system();
    if (system_little_endian != erg->little_endian) {
        /* Need to swap bytes */
        uint8_t* data_ptr = (uint8_t*)erg->raw_data;
        for (size_t row = 0; row < erg->sample_count; row++) {
            for (size_t sig = 0; sig < erg->signal_count; sig++) {
                ERGSignal* signal = &erg->signals[sig];

                switch (signal->type_size) {
                case 2:
                    *(uint16_t*)data_ptr = swap_uint16(*(uint16_t*)data_ptr);
                    break;
                case 4:
                    *(uint32_t*)data_ptr = swap_uint32(*(uint32_t*)data_ptr);
                    break;
                case 8:
                    *(uint64_t*)data_ptr = swap_uint64(*(uint64_t*)data_ptr);
                    break;
                    /* 1-byte types don't need swapping */
                }

                data_ptr += signal->type_size;
            }
        }
    }
}

int erg_find_signal_index(const ERG* erg, const char* signal_name) {
    for (size_t i = 0; i < erg->signal_count; i++) {
        if (strcmp(erg->signals[i].name, signal_name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

const ERGSignal* erg_get_signal_info(const ERG* erg, const char* signal_name) {
    int index = erg_find_signal_index(erg, signal_name);
    if (index < 0)
        return NULL;
    return &erg->signals[index];
}

const void* erg_get_signal(const ERG* erg, const char* signal_name, size_t* length) {
    int index = erg_find_signal_index(erg, signal_name);
    if (index < 0) {
        *length = 0;
        return NULL;
    }

    /* Calculate offset to this signal's data in each row */
    size_t offset = 0;
    for (int i = 0; i < index; i++) {
        offset += erg->signals[i].type_size;
    }

    /* For interleaved data, we need to extract the signal column
     * Allocate a continuous array */
    ERGSignal* sig         = &erg->signals[index];
    void*      signal_data = malloc(sig->type_size * erg->sample_count);
    if (!signal_data) {
        fprintf(stderr, "FATAL: Failed to allocate signal data array\n");
        exit(1);
    }

    /* Extract signal values from interleaved rows */
    uint8_t* src = (uint8_t*)erg->raw_data + offset;
    uint8_t* dst = (uint8_t*)signal_data;

    for (size_t row = 0; row < erg->sample_count; row++) {
        memcpy(dst, src, sig->type_size);
        src += erg->row_size;
        dst += sig->type_size;
    }

    *length = erg->sample_count;
    return signal_data;
}

double* erg_get_signal_as_double(const ERG* erg, const char* signal_name, size_t* length) {
    int index = erg_find_signal_index(erg, signal_name);
    if (index < 0) {
        *length = 0;
        return NULL;
    }

    ERGSignal* sig = &erg->signals[index];

    /* Get raw signal data */
    size_t      sample_count;
    const void* raw_data = erg_get_signal(erg, signal_name, &sample_count);
    if (!raw_data) {
        *length = 0;
        return NULL;
    }

    /* Allocate double array */
    double* result = malloc(sample_count * sizeof(double));
    if (!result) {
        fprintf(stderr, "FATAL: Failed to allocate double array\n");
        free((void*)raw_data);
        exit(1);
    }

    /* Convert to double with scaling */
    for (size_t i = 0; i < sample_count; i++) {
        double value = 0.0;

        switch (sig->type) {
        case ERG_FLOAT:
            value = ((float*)raw_data)[i];
            break;
        case ERG_DOUBLE:
            value = ((double*)raw_data)[i];
            break;
        case ERG_LONGLONG:
            value = (double)((int64_t*)raw_data)[i];
            break;
        case ERG_ULONGLONG:
            value = (double)((uint64_t*)raw_data)[i];
            break;
        case ERG_INT:
            value = (double)((int32_t*)raw_data)[i];
            break;
        case ERG_UINT:
            value = (double)((uint32_t*)raw_data)[i];
            break;
        case ERG_SHORT:
            value = (double)((int16_t*)raw_data)[i];
            break;
        case ERG_USHORT:
            value = (double)((uint16_t*)raw_data)[i];
            break;
        case ERG_CHAR:
            value = (double)((int8_t*)raw_data)[i];
            break;
        case ERG_UCHAR:
            value = (double)((uint8_t*)raw_data)[i];
            break;
        default:
            value = 0.0;
            break;
        }

        /* Apply scaling */
        result[i] = value * sig->factor + sig->offset;
    }

    free((void*)raw_data);
    *length = sample_count;
    return result;
}

void erg_free(ERG* erg) {
    if (!erg)
        return;

    /* Free path */
    if (erg->erg_path) {
        free(erg->erg_path);
        erg->erg_path = NULL;
    }

    /* Free signals */
    if (erg->signals) {
        for (size_t i = 0; i < erg->signal_count; i++) {
            if (erg->signals[i].name)
                free(erg->signals[i].name);
            if (erg->signals[i].unit)
                free(erg->signals[i].unit);
        }
        free(erg->signals);
        erg->signals = NULL;
    }

    /* Free raw data */
    if (erg->raw_data) {
        free(erg->raw_data);
        erg->raw_data = NULL;
    }

    /* Free info file */
    if (erg->info) {
        infofile_free(erg->info);
        free(erg->info);
        erg->info = NULL;
    }

    erg->signal_count = 0;
    erg->sample_count = 0;
}
