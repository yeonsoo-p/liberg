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


void erg_init(ERG* erg, const char* erg_file_path, ThreadPool* pool) {
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

    /* Store thread pool reference (adaptive thread count decided after parsing) */
    erg->thread_pool = pool;
    erg->num_threads = 1;  /* Default to single-threaded, updated in erg_parse() */
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

    /* Adaptive threading: decide how many threads to use based on sample count */
    if (erg->thread_pool == NULL || erg->sample_count < MIN_SAMPLES_PER_THREAD) {
        erg->num_threads = 1;  /* Single-threaded */
    } else {
        erg->num_threads = 2;  /* Use 2 threads for now */
        /* Future: more sophisticated logic based on sample_count, signal_count, pool size */
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

/* SIMD-optimized signal extraction functions */

/* Extract signal using SIMD for 4-byte elements (float, int, uint) */
static void extract_signal_4byte_simd(const uint8_t* row_data, uint8_t* signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;

    /* Process 8 samples at a time with AVX2 */
    size_t simd_samples = (sample_count / 8) * 8;

    for (size_t i = 0; i < simd_samples; i += 8) {
        /* Gather 8 int32 values from different rows */
        __m256i data = _mm256_set_epi32(
            *(const int32_t*)(src + (i + 7) * row_size),
            *(const int32_t*)(src + (i + 6) * row_size),
            *(const int32_t*)(src + (i + 5) * row_size),
            *(const int32_t*)(src + (i + 4) * row_size),
            *(const int32_t*)(src + (i + 3) * row_size),
            *(const int32_t*)(src + (i + 2) * row_size),
            *(const int32_t*)(src + (i + 1) * row_size),
            *(const int32_t*)(src + (i + 0) * row_size)
        );
        _mm256_storeu_si256((__m256i*)(signal_data + i * 4), data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 4, src + i * row_size, 4);
    }
}

/* Extract signal using SIMD for 8-byte elements (double, int64, uint64) */
static void extract_signal_8byte_simd(const uint8_t* row_data, uint8_t* signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;

    /* Process 4 samples at a time with AVX2 */
    size_t simd_samples = (sample_count / 4) * 4;

    for (size_t i = 0; i < simd_samples; i += 4) {
        /* Gather 4 int64 values from different rows */
        __m256i data = _mm256_set_epi64x(
            *(const int64_t*)(src + (i + 3) * row_size),
            *(const int64_t*)(src + (i + 2) * row_size),
            *(const int64_t*)(src + (i + 1) * row_size),
            *(const int64_t*)(src + (i + 0) * row_size)
        );
        _mm256_storeu_si256((__m256i*)(signal_data + i * 8), data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 8, src + i * row_size, 8);
    }
}

/* Extract signal using SIMD for 2-byte elements (short, ushort) */
static void extract_signal_2byte_simd(const uint8_t* row_data, uint8_t* signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;

    /* Process 16 samples at a time with AVX2 */
    size_t simd_samples = (sample_count / 16) * 16;

    for (size_t i = 0; i < simd_samples; i += 16) {
        /* Gather 16 int16 values from different rows */
        __m256i data = _mm256_set_epi16(
            *(const int16_t*)(src + (i + 15) * row_size),
            *(const int16_t*)(src + (i + 14) * row_size),
            *(const int16_t*)(src + (i + 13) * row_size),
            *(const int16_t*)(src + (i + 12) * row_size),
            *(const int16_t*)(src + (i + 11) * row_size),
            *(const int16_t*)(src + (i + 10) * row_size),
            *(const int16_t*)(src + (i + 9) * row_size),
            *(const int16_t*)(src + (i + 8) * row_size),
            *(const int16_t*)(src + (i + 7) * row_size),
            *(const int16_t*)(src + (i + 6) * row_size),
            *(const int16_t*)(src + (i + 5) * row_size),
            *(const int16_t*)(src + (i + 4) * row_size),
            *(const int16_t*)(src + (i + 3) * row_size),
            *(const int16_t*)(src + (i + 2) * row_size),
            *(const int16_t*)(src + (i + 1) * row_size),
            *(const int16_t*)(src + (i + 0) * row_size)
        );
        _mm256_storeu_si256((__m256i*)(signal_data + i * 2), data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 2, src + i * row_size, 2);
    }
}

/* Generic scalar extraction for other sizes */
static void extract_signal_scalar(const uint8_t* row_data, uint8_t* signal_data,
                                  size_t sample_count, size_t signal_offset,
                                  size_t element_size, size_t row_size) {
    const uint8_t* src = row_data + signal_offset;

    for (size_t i = 0; i < sample_count; i++) {
        memcpy(signal_data + i * element_size, src + i * row_size, element_size);
    }
}

/* ============================================================================
 * MULTI-THREADED EXTRACTION INFRASTRUCTURE
 * ============================================================================ */

/* Thread work structure */
typedef struct {
    const uint8_t* row_data;
    uint8_t*       signal_data;
    size_t         start_sample;
    size_t         end_sample;
    size_t         signal_offset;
    size_t         element_size;
    size_t         row_size;
} ExtractWork;

/* Generic extraction that chooses SIMD or scalar based on element size */
static void extract_signal_generic(const uint8_t* row_data, uint8_t* signal_data,
                                   size_t start_sample, size_t end_sample,
                                   size_t signal_offset, size_t element_size, size_t row_size) {
    size_t         sample_count = end_sample - start_sample;
    const uint8_t* src          = row_data + start_sample * row_size;
    uint8_t*       dest         = signal_data + start_sample * element_size;

    switch (element_size) {
    case 2:
        extract_signal_2byte_simd(src, dest, sample_count, signal_offset, row_size);
        break;
    case 4:
        extract_signal_4byte_simd(src, dest, sample_count, signal_offset, row_size);
        break;
    case 8:
        extract_signal_8byte_simd(src, dest, sample_count, signal_offset, row_size);
        break;
    default:
        extract_signal_scalar(src, dest, sample_count, signal_offset, element_size, row_size);
        break;
    }
}

/* Worker function for thread pool */
static void* extract_worker_pool(void* arg) {
    ExtractWork* work = (ExtractWork*)arg;
    extract_signal_generic(work->row_data, work->signal_data,
                          work->start_sample, work->end_sample,
                          work->signal_offset, work->element_size, work->row_size);
    return NULL;
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

    ERGSignal* sig = &erg->signals[index];

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

    /* Check if we should use threading (decision made in erg_parse) */
    if (erg->num_threads > 1 && erg->thread_pool) {
        /* Multi-threaded extraction using thread pool */
        int num_threads = erg->num_threads;

        /* Prepare work items for each thread */
        ExtractWork* work = malloc(num_threads * sizeof(ExtractWork));
        void** work_args = malloc(num_threads * sizeof(void*));

        if (!work || !work_args) {
            fprintf(stderr, "FATAL: Failed to allocate work array\n");
            free(result);
            free(work);
            free(work_args);
            exit(1);
        }

        /* Divide work among threads */
        size_t samples_per_thread = erg->sample_count / num_threads;
        size_t remainder = erg->sample_count % num_threads;

        for (int i = 0; i < num_threads; i++) {
            work[i].row_data = row_data;
            work[i].signal_data = (uint8_t*)result;
            work[i].start_sample = i * samples_per_thread + (i < (int)remainder ? i : remainder);
            work[i].end_sample = work[i].start_sample + samples_per_thread + (i < (int)remainder ? 1 : 0);
            work[i].signal_offset = offset;
            work[i].element_size = sig->type_size;
            work[i].row_size = erg->row_size;
            work_args[i] = &work[i];
        }

        /* Submit work to thread pool and wait for completion */
        thread_pool_submit(erg->thread_pool, extract_worker_pool, work_args, num_threads);
        thread_pool_wait(erg->thread_pool);

        free(work);
        free(work_args);
    } else {
        /* Single-threaded extraction */
        extract_signal_generic(row_data, (uint8_t*)result, 0, erg->sample_count,
                             offset, sig->type_size, erg->row_size);
    }

    return result;
}

double* erg_get_signal_as_double(const ERG* erg, const char* signal_name) {
    int index = erg_find_signal_index(erg, signal_name);
    if (index < 0) {
        return NULL;
    }

    /* Get raw typed data first (automatically uses threading if beneficial) */
    void* raw_data = erg_get_signal(erg, signal_name);
    if (!raw_data) {
        return NULL;
    }

    ERGSignal* sig = &erg->signals[index];

    /* Allocate double array */
    double* result = malloc(erg->sample_count * sizeof(double));
    if (!result) {
        fprintf(stderr, "FATAL: Failed to allocate double array (%zu bytes)\n",
                erg->sample_count * sizeof(double));
        free(raw_data);
        exit(1);
    }

    /* Convert to double with scaling */
    for (size_t i = 0; i < erg->sample_count; i++) {
        double value = 0.0;

        switch (sig->type) {
        case ERG_FLOAT:
            value = (double)((float*)raw_data)[i];
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
        case ERG_BYTES:
        default:
            value = 0.0;
            break;
        }

        /* Apply scaling */
        result[i] = value * sig->factor + sig->offset;
    }

    free(raw_data);
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
