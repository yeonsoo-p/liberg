#include <erg.h>
#include <infofile.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <immintrin.h>
#include <cpuid.h>
#endif

/* Memory-mapped file support */
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

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
        /* Invalid Bytes format - log warning */
        fprintf(stderr, "WARNING: Invalid Bytes format: '%s'\n", type_str);
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



void erg_init(ERG* erg, const char* erg_file_path) {
    memset(erg, 0, sizeof(ERG));

    /* Initialize arena for metadata strings (64KB initial, grows as needed) */
    arena_init(&erg->metadata_arena, 64 * 1024);

    /* Store ERG file path in arena */
    erg->erg_path = arena_strdup(&erg->metadata_arena, erg_file_path);

    /* Initialize info file structure */
    erg->info = malloc(sizeof(InfoFile));
    if (!erg->info) {
        fprintf(stderr, "FATAL: Failed to allocate InfoFile structure\n");
        exit(1);
    }

    /* Initialize memory-mapping fields to NULL/invalid */
    erg->mapped_data = NULL;
    erg->mapped_size = 0;
#ifdef _WIN32
    erg->file_handle = INVALID_HANDLE_VALUE;
    erg->mapping_handle = NULL;
#else
    erg->file_descriptor = -1;
#endif
}

void erg_parse(ERG* erg) {
    /* Build info file path (.erg.info) using arena */
    size_t info_path_len = strlen(erg->erg_path) + 6; /* +".info" */
    char*  info_path     = arena_alloc(&erg->metadata_arena, info_path_len);
    snprintf(info_path, info_path_len, "%s.info", erg->erg_path);

    /* Parse info file */
    infofile_init(erg->info);
    infofile_parse_file(info_path, erg->info);
    /* No need to free - arena will handle it */

    /* Get byte order - only support little-endian */
    const char* byte_order = infofile_get(erg->info, "File.ByteOrder");
    if (!byte_order) {
        fprintf(stderr, "FATAL: File.ByteOrder not found in ERG info file\n");
        exit(1);
    }
    if (strcmp(byte_order, "LittleEndian") != 0) {
        fprintf(stderr, "FATAL: Only little-endian ERG files are supported (found: %s)\n", byte_order);
        exit(1);
    }
    erg->little_endian = 1;

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

        /* Get signal name - use arena */
        snprintf(key_buffer, sizeof(key_buffer), "File.At.%zu.Name", i + 1);
        const char* name = infofile_get(erg->info, key_buffer);
        sig->name        = arena_strdup(&erg->metadata_arena, name);

        /* Get data type */
        snprintf(key_buffer, sizeof(key_buffer), "File.At.%zu.Type", i + 1);
        const char* type_str = infofile_get(erg->info, key_buffer);
        if (!type_str) {
            fprintf(stderr, "FATAL: Data type not found for signal %s\n", name);
            exit(1);
        }
        sig->type = parse_data_type(type_str, &sig->type_size);

        /* Get unit - use arena */
        snprintf(key_buffer, sizeof(key_buffer), "Quantity.%s.Unit", name);
        const char* unit = infofile_get(erg->info, key_buffer);
        if (unit) {
            sig->unit = arena_strdup(&erg->metadata_arena, unit);
        } else {
            sig->unit = arena_strdup(&erg->metadata_arena, "");
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

    /* Store data offset (after header) */
    erg->data_offset = ERG_HEADER_SIZE;

    /* Calculate data size and sample count */
    erg->data_size = file_size - ERG_HEADER_SIZE;

    /* Bug fix #1: Check for zero row size */
    if (erg->row_size == 0) {
        fprintf(stderr, "FATAL: Invalid row size (0 bytes) - no signals or signal metadata error\n");
        fclose(fp);
        exit(1);
    }

    /* Bug fix #3: Validate data alignment */
    if (erg->data_size % erg->row_size != 0) {
        fprintf(stderr, "WARNING: Data size (%zu) not evenly divisible by row size (%zu)\n",
                erg->data_size, erg->row_size);
        fprintf(stderr, "         File may be corrupt or truncated (remainder: %zu bytes)\n",
                erg->data_size % erg->row_size);
    }

    erg->sample_count = erg->data_size / erg->row_size;

    /* Close the FILE* - we'll use memory mapping instead */
    fclose(fp);

    /* Create memory-mapped file for efficient access */
#ifdef _WIN32
    /* Windows memory mapping */
    erg->file_handle = CreateFileA(
        erg->erg_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (erg->file_handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "FATAL: Failed to open ERG file for memory mapping: %s\n", erg->erg_path);
        exit(1);
    }

    erg->mapped_size = (size_t)file_size;

    erg->mapping_handle = CreateFileMappingA(
        erg->file_handle,
        NULL,
        PAGE_READONLY,
        0,
        0,  /* Map entire file */
        NULL
    );

    if (!erg->mapping_handle) {
        fprintf(stderr, "FATAL: Failed to create file mapping for ERG file\n");
        CloseHandle(erg->file_handle);
        exit(1);
    }

    erg->mapped_data = MapViewOfFile(
        erg->mapping_handle,
        FILE_MAP_READ,
        0,
        0,
        0  /* Map entire file */
    );

    if (!erg->mapped_data) {
        fprintf(stderr, "FATAL: Failed to map view of ERG file\n");
        CloseHandle(erg->mapping_handle);
        CloseHandle(erg->file_handle);
        exit(1);
    }

#else
    /* POSIX memory mapping */
    erg->file_descriptor = open(erg->erg_path, O_RDONLY);
    if (erg->file_descriptor == -1) {
        fprintf(stderr, "FATAL: Failed to open ERG file for memory mapping: %s\n", erg->erg_path);
        exit(1);
    }

    erg->mapped_size = (size_t)file_size;

    erg->mapped_data = mmap(
        NULL,
        erg->mapped_size,
        PROT_READ,
        MAP_PRIVATE,
        erg->file_descriptor,
        0
    );

    if (erg->mapped_data == MAP_FAILED) {
        fprintf(stderr, "FATAL: Failed to memory-map ERG file\n");
        close(erg->file_descriptor);
        exit(1);
    }
#endif

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

/* ============================================================================
 * SIGNAL SCALING
 * ============================================================================ */

/* Apply scaling to signal data in-place */
static void apply_signal_scaling(void* data, const ERGSignal* sig, size_t sample_count) {
    if (sig->factor == 1.0 && sig->offset == 0.0) {
        return;  /* No scaling needed */
    }

    switch (sig->type) {
    case ERG_FLOAT: {
        float* fdata = (float*)data;
        float factor_f = (float)sig->factor;
        float offset_f = (float)sig->offset;
        for (size_t i = 0; i < sample_count; i++) {
            fdata[i] = fdata[i] * factor_f + offset_f;
        }
        break;
    }
    case ERG_DOUBLE: {
        double* ddata = (double*)data;
        for (size_t i = 0; i < sample_count; i++) {
            ddata[i] = ddata[i] * sig->factor + sig->offset;
        }
        break;
    }
    case ERG_INT: {
        int32_t* idata = (int32_t*)data;
        int32_t factor_i = (int32_t)sig->factor;
        int32_t offset_i = (int32_t)sig->offset;
        for (size_t i = 0; i < sample_count; i++) {
            idata[i] = idata[i] * factor_i + offset_i;
        }
        break;
    }
    case ERG_UINT: {
        uint32_t* udata = (uint32_t*)data;
        uint32_t factor_u = (uint32_t)sig->factor;
        uint32_t offset_u = (uint32_t)sig->offset;
        for (size_t i = 0; i < sample_count; i++) {
            udata[i] = udata[i] * factor_u + offset_u;
        }
        break;
    }
    case ERG_SHORT: {
        int16_t* sdata = (int16_t*)data;
        int16_t factor_s = (int16_t)sig->factor;
        int16_t offset_s = (int16_t)sig->offset;
        for (size_t i = 0; i < sample_count; i++) {
            sdata[i] = sdata[i] * factor_s + offset_s;
        }
        break;
    }
    case ERG_USHORT: {
        uint16_t* usdata = (uint16_t*)data;
        uint16_t factor_us = (uint16_t)sig->factor;
        uint16_t offset_us = (uint16_t)sig->offset;
        for (size_t i = 0; i < sample_count; i++) {
            usdata[i] = usdata[i] * factor_us + offset_us;
        }
        break;
    }
    case ERG_LONGLONG: {
        int64_t* lldata = (int64_t*)data;
        int64_t factor_ll = (int64_t)sig->factor;
        int64_t offset_ll = (int64_t)sig->offset;
        for (size_t i = 0; i < sample_count; i++) {
            lldata[i] = lldata[i] * factor_ll + offset_ll;
        }
        break;
    }
    case ERG_ULONGLONG: {
        uint64_t* ulldata = (uint64_t*)data;
        uint64_t factor_ull = (uint64_t)sig->factor;
        uint64_t offset_ull = (uint64_t)sig->offset;
        for (size_t i = 0; i < sample_count; i++) {
            ulldata[i] = ulldata[i] * factor_ull + offset_ull;
        }
        break;
    }
    case ERG_CHAR: {
        int8_t* cdata = (int8_t*)data;
        int8_t factor_c = (int8_t)sig->factor;
        int8_t offset_c = (int8_t)sig->offset;
        for (size_t i = 0; i < sample_count; i++) {
            cdata[i] = cdata[i] * factor_c + offset_c;
        }
        break;
    }
    case ERG_UCHAR: {
        uint8_t* ucdata = (uint8_t*)data;
        uint8_t factor_uc = (uint8_t)sig->factor;
        uint8_t offset_uc = (uint8_t)sig->offset;
        for (size_t i = 0; i < sample_count; i++) {
            ucdata[i] = ucdata[i] * factor_uc + offset_uc;
        }
        break;
    }
    case ERG_BYTES:
    default:
        /* No scaling for byte arrays */
        break;
    }
}

/* ============================================================================
 * PUBLIC API
 * ============================================================================ */

void* erg_get_signal(const ERG* erg, const char* signal_name) {
    int index = erg_find_signal_index(erg, signal_name);
    if (index < 0) {
        return NULL;
    }

    /* Handle empty data case */
    if (erg->sample_count == 0) {
        return NULL;
    }

    /* Calculate offset to this signal's data in each row */
    size_t offset = 0;
    for (int i = 0; i < index; i++) {
        offset += erg->signals[i].type_size;
    }

    const ERGSignal* sig = &erg->signals[index];

    /* Allocate output array for signal data */
    void* result = malloc(erg->sample_count * sig->type_size);
    if (!result) {
        fprintf(stderr, "FATAL: Failed to allocate signal array (%zu bytes)\n",
                erg->sample_count * sig->type_size);
        exit(1);
    }

    /* Access data directly from memory-mapped file (zero-copy) */
    if (!erg->mapped_data) {
        fprintf(stderr, "FATAL: ERG file not memory-mapped\n");
        free(result);
        exit(1);
    }

    /* Point to the start of the data region in the mapped file */
    const uint8_t* row_data = (const uint8_t*)erg->mapped_data + erg->data_offset;

    /* Extract signal data from row-major layout */
    const uint8_t* src = row_data + offset;
    uint8_t* dest = (uint8_t*)result;
    for (size_t i = 0; i < erg->sample_count; i++) {
        memcpy(dest + i * sig->type_size, src + i * erg->row_size, sig->type_size);
    }

    /* Apply scaling if needed */
    apply_signal_scaling(result, sig, erg->sample_count);

    return result;
}

void erg_free(ERG* erg) {
    if (!erg)
        return;

    /* Unmap memory-mapped file */
    if (erg->mapped_data) {
#ifdef _WIN32
        UnmapViewOfFile(erg->mapped_data);
        erg->mapped_data = NULL;

        if (erg->mapping_handle) {
            CloseHandle(erg->mapping_handle);
            erg->mapping_handle = NULL;
        }

        if (erg->file_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(erg->file_handle);
            erg->file_handle = INVALID_HANDLE_VALUE;
        }
#else
        munmap(erg->mapped_data, erg->mapped_size);
        erg->mapped_data = NULL;

        if (erg->file_descriptor != -1) {
            close(erg->file_descriptor);
            erg->file_descriptor = -1;
        }
#endif
        erg->mapped_size = 0;
    }

    /* Free arena - this frees erg_path, all signal names, and all units in one call! */
    arena_free(&erg->metadata_arena);

    /* Free signals array (the ERGSignal structs themselves, not the strings) */
    if (erg->signals) {
        free(erg->signals);
        erg->signals = NULL;
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
