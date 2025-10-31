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

/* CPU feature detection using CPUID */
static ERGSIMDLevel detect_simd_level(void) {
#ifdef _MSC_VER
    int cpu_info[4];

    /* Check for AVX512F (AVX-512 Foundation) */
    __cpuidex(cpu_info, 7, 0);
    if (cpu_info[1] & (1 << 16)) {  /* EBX bit 16 = AVX512F */
        /* Also check OSXSAVE and that OS supports AVX512 */
        __cpuid(cpu_info, 1);
        if ((cpu_info[2] & (1 << 27)) != 0) {  /* OSXSAVE */
            unsigned long long xcr0 = _xgetbv(0);
            if ((xcr0 & 0xE6) == 0xE6) {  /* Check AVX512 state */
                return ERG_SIMD_AVX512;
            }
        }
    }

    /* Check for AVX2 */
    __cpuidex(cpu_info, 7, 0);
    if (cpu_info[1] & (1 << 5)) {  /* EBX bit 5 = AVX2 */
        /* Also check OSXSAVE and that OS supports AVX */
        __cpuid(cpu_info, 1);
        if ((cpu_info[2] & (1 << 27)) != 0) {  /* OSXSAVE */
            unsigned long long xcr0 = _xgetbv(0);
            if ((xcr0 & 0x6) == 0x6) {  /* Check AVX state */
                return ERG_SIMD_AVX2;
            }
        }
    }

    /* Check for SSE2 (always available on x64) */
    __cpuid(cpu_info, 1);
    if (cpu_info[3] & (1 << 26)) {  /* EDX bit 26 = SSE2 */
        return ERG_SIMD_SSE2;
    }
#else
    unsigned int eax, ebx, ecx, edx;

    /* Check for AVX512F */
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1 << 16)) {  /* AVX512F */
            /* Check OSXSAVE and OS support */
            if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
                if (ecx & (1 << 27)) {  /* OSXSAVE */
                    unsigned long long xcr0;
                    __asm__ ("xgetbv" : "=a"(xcr0) : "c"(0) : "%edx");
                    if ((xcr0 & 0xE6) == 0xE6) {
                        return ERG_SIMD_AVX512;
                    }
                }
            }
        }
    }

    /* Check for AVX2 */
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1 << 5)) {  /* AVX2 */
            /* Check OSXSAVE and OS support */
            if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
                if (ecx & (1 << 27)) {  /* OSXSAVE */
                    unsigned long long xcr0;
                    __asm__ ("xgetbv" : "=a"(xcr0) : "c"(0) : "%edx");
                    if ((xcr0 & 0x6) == 0x6) {
                        return ERG_SIMD_AVX2;
                    }
                }
            }
        }
    }

    /* Check for SSE2 */
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (edx & (1 << 26)) {  /* SSE2 */
            return ERG_SIMD_SSE2;
        }
    }
#endif

    return ERG_SIMD_NONE;
}


void erg_init(ERG* erg, const char* erg_file_path) {
    memset(erg, 0, sizeof(ERG));

    /* Detect best available SIMD instruction set */
    erg->simd_level = detect_simd_level();

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

/* SIMD-optimized signal extraction functions */

/* ============================================================================
 * SSE2 IMPLEMENTATIONS (128-bit, available on all x64 CPUs)
 * ============================================================================ */

/* SSE2: Extract 4-byte elements (4 samples at a time) - OPTIMIZED */
static void extract_signal_4byte_sse2(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 4) * 4;

    /* Optimized: Use individual loads and unpack instead of _mm_set which is inefficient */
    for (size_t i = 0; i < simd_samples; i += 4) {
        /* Load each 32-bit value individually */
        __m128i val0 = _mm_cvtsi32_si128(*(const int32_t*)(src + (i + 0) * row_size));
        __m128i val1 = _mm_cvtsi32_si128(*(const int32_t*)(src + (i + 1) * row_size));
        __m128i val2 = _mm_cvtsi32_si128(*(const int32_t*)(src + (i + 2) * row_size));
        __m128i val3 = _mm_cvtsi32_si128(*(const int32_t*)(src + (i + 3) * row_size));

        /* Combine using unpack operations - more efficient than set */
        __m128i val01 = _mm_unpacklo_epi32(val0, val1);  /* [val0, val1, 0, 0] */
        __m128i val23 = _mm_unpacklo_epi32(val2, val3);  /* [val2, val3, 0, 0] */
        __m128i data = _mm_unpacklo_epi64(val01, val23); /* [val0, val1, val2, val3] */

        _mm_storeu_si128((__m128i*)(signal_data + i * 4), data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 4, src + i * row_size, 4);
    }
}

/* SSE2: Extract 8-byte elements (2 samples at a time) - OPTIMIZED */
static void extract_signal_8byte_sse2(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 2) * 2;

    /* Optimized: Use _mm_loadl_epi64 for better performance */
    for (size_t i = 0; i < simd_samples; i += 2) {
        /* Load two 64-bit values using loadl and unpacklo */
        __m128i val0 = _mm_loadl_epi64((const __m128i*)(src + (i + 0) * row_size));
        __m128i val1 = _mm_loadl_epi64((const __m128i*)(src + (i + 1) * row_size));
        __m128i data = _mm_unpacklo_epi64(val0, val1);  /* [val0, val1] */

        _mm_storeu_si128((__m128i*)(signal_data + i * 8), data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 8, src + i * row_size, 8);
    }
}

/* SSE2: Extract 2-byte elements (8 samples at a time) - OPTIMIZED */
static void extract_signal_2byte_sse2(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 8) * 8;

    /* Optimized: Use individual loads and interleave with unpacks */
    for (size_t i = 0; i < simd_samples; i += 8) {
        /* Load 8 int16 values using insert operations */
        __m128i val0 = _mm_cvtsi32_si128(*(const uint16_t*)(src + (i + 0) * row_size));
        __m128i val1 = _mm_cvtsi32_si128(*(const uint16_t*)(src + (i + 1) * row_size));
        __m128i val2 = _mm_cvtsi32_si128(*(const uint16_t*)(src + (i + 2) * row_size));
        __m128i val3 = _mm_cvtsi32_si128(*(const uint16_t*)(src + (i + 3) * row_size));
        __m128i val4 = _mm_cvtsi32_si128(*(const uint16_t*)(src + (i + 4) * row_size));
        __m128i val5 = _mm_cvtsi32_si128(*(const uint16_t*)(src + (i + 5) * row_size));
        __m128i val6 = _mm_cvtsi32_si128(*(const uint16_t*)(src + (i + 6) * row_size));
        __m128i val7 = _mm_cvtsi32_si128(*(const uint16_t*)(src + (i + 7) * row_size));

        /* Interleave pairs using unpack */
        __m128i val01 = _mm_unpacklo_epi16(val0, val1);
        __m128i val23 = _mm_unpacklo_epi16(val2, val3);
        __m128i val45 = _mm_unpacklo_epi16(val4, val5);
        __m128i val67 = _mm_unpacklo_epi16(val6, val7);

        /* Combine pairs into quads */
        __m128i val0123 = _mm_unpacklo_epi32(val01, val23);
        __m128i val4567 = _mm_unpacklo_epi32(val45, val67);

        /* Final combine */
        __m128i data = _mm_unpacklo_epi64(val0123, val4567);

        _mm_storeu_si128((__m128i*)(signal_data + i * 2), data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 2, src + i * row_size, 2);
    }
}

/* SSE2: Extract 1-byte elements (16 samples at a time) - NEW */
static void extract_signal_1byte_sse2(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 16) * 16;

    /* Process 16 bytes at a time */
    for (size_t i = 0; i < simd_samples; i += 16) {
        /* Load 16 individual bytes */
        __m128i val0 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 0) * row_size));
        __m128i val1 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 1) * row_size));
        __m128i val2 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 2) * row_size));
        __m128i val3 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 3) * row_size));
        __m128i val4 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 4) * row_size));
        __m128i val5 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 5) * row_size));
        __m128i val6 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 6) * row_size));
        __m128i val7 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 7) * row_size));
        __m128i val8 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 8) * row_size));
        __m128i val9 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 9) * row_size));
        __m128i val10 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 10) * row_size));
        __m128i val11 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 11) * row_size));
        __m128i val12 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 12) * row_size));
        __m128i val13 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 13) * row_size));
        __m128i val14 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 14) * row_size));
        __m128i val15 = _mm_cvtsi32_si128(*(const uint8_t*)(src + (i + 15) * row_size));

        /* Interleave bytes progressively */
        __m128i val01 = _mm_unpacklo_epi8(val0, val1);
        __m128i val23 = _mm_unpacklo_epi8(val2, val3);
        __m128i val45 = _mm_unpacklo_epi8(val4, val5);
        __m128i val67 = _mm_unpacklo_epi8(val6, val7);
        __m128i val89 = _mm_unpacklo_epi8(val8, val9);
        __m128i val1011 = _mm_unpacklo_epi8(val10, val11);
        __m128i val1213 = _mm_unpacklo_epi8(val12, val13);
        __m128i val1415 = _mm_unpacklo_epi8(val14, val15);

        /* Combine into 16-bit pairs */
        __m128i val0123 = _mm_unpacklo_epi16(val01, val23);
        __m128i val4567 = _mm_unpacklo_epi16(val45, val67);
        __m128i val891011 = _mm_unpacklo_epi16(val89, val1011);
        __m128i val12131415 = _mm_unpacklo_epi16(val1213, val1415);

        /* Combine into 32-bit quads */
        __m128i val01234567 = _mm_unpacklo_epi32(val0123, val4567);
        __m128i val89101112131415 = _mm_unpacklo_epi32(val891011, val12131415);

        /* Final combine */
        __m128i data = _mm_unpacklo_epi64(val01234567, val89101112131415);

        _mm_storeu_si128((__m128i*)(signal_data + i), data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        signal_data[i] = src[i * row_size];
    }
}

/* ============================================================================
 * AVX2 IMPLEMENTATIONS (256-bit, available on most modern CPUs)
 * ============================================================================ */

/* AVX2: Extract 4-byte elements (8 samples at a time with gather) */
static void extract_signal_4byte_avx2(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 8) * 8;

    /* Create index vector for gather: [0, row_size, 2*row_size, ..., 7*row_size] */
    __m256i vindex = _mm256_setr_epi32(
        0 * row_size,
        1 * row_size,
        2 * row_size,
        3 * row_size,
        4 * row_size,
        5 * row_size,
        6 * row_size,
        7 * row_size
    );

    for (size_t i = 0; i < simd_samples; i += 8) {
        /* Gather 8 int32 values from strided memory locations */
        __m256i data = _mm256_i32gather_epi32((const int*)(src + i * row_size), vindex, 1);
        _mm256_storeu_si256((__m256i*)(signal_data + i * 4), data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 4, src + i * row_size, 4);
    }
}

/* AVX2: Extract 8-byte elements (4 samples at a time with gather) */
static void extract_signal_8byte_avx2(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 4) * 4;

    /* Create index vector for gather: [0, row_size, 2*row_size, 3*row_size] */
    __m256i vindex = _mm256_setr_epi64x(
        0 * row_size,
        1 * row_size,
        2 * row_size,
        3 * row_size
    );

    for (size_t i = 0; i < simd_samples; i += 4) {
        /* Gather 4 int64 values from strided memory locations */
        __m256i data = _mm256_i64gather_epi64((const long long*)(src + i * row_size), vindex, 1);
        _mm256_storeu_si256((__m256i*)(signal_data + i * 8), data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 8, src + i * row_size, 8);
    }
}

/* AVX2: Extract 2-byte elements (16 samples at a time) - OPTIMIZED */
static void extract_signal_2byte_avx2(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 16) * 16;

    /* Optimized: Use packs instead of packus for signed/unsigned consistency */
    __m256i vindex = _mm256_setr_epi32(
        0 * row_size,
        1 * row_size,
        2 * row_size,
        3 * row_size,
        4 * row_size,
        5 * row_size,
        6 * row_size,
        7 * row_size
    );

    for (size_t i = 0; i < simd_samples; i += 16) {
        /* Gather 16-bit values by reading 32 bits */
        __m256i data_lo = _mm256_i32gather_epi32((const int*)(src + i * row_size), vindex, 1);
        __m256i data_hi = _mm256_i32gather_epi32((const int*)(src + (i + 8) * row_size), vindex, 1);

        /* Mask to keep only lower 16 bits */
        __m256i mask = _mm256_set1_epi32(0x0000FFFF);
        data_lo = _mm256_and_si256(data_lo, mask);
        data_hi = _mm256_and_si256(data_hi, mask);

        /* Pack 32-bit to 16-bit using packs - more efficient than packus with mask */
        __m256i packed = _mm256_packs_epi32(data_lo, data_hi);

        /* Fix lane ordering */
        packed = _mm256_permute4x64_epi64(packed, 0xD8);  /* [0,2,1,3] */

        _mm256_storeu_si256((__m256i*)(signal_data + i * 2), packed);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 2, src + i * row_size, 2);
    }
}

/* AVX2: Extract 1-byte elements (32 samples at a time) - NEW */
static void extract_signal_1byte_avx2(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                      size_t sample_count, size_t signal_offset,
                                      size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 32) * 32;

    /* Create index vectors for gather */
    __m256i vindex = _mm256_setr_epi32(
        0 * row_size,
        1 * row_size,
        2 * row_size,
        3 * row_size,
        4 * row_size,
        5 * row_size,
        6 * row_size,
        7 * row_size
    );

    for (size_t i = 0; i < simd_samples; i += 32) {
        /* Gather 32 bytes using four 8-element gathers */
        __m256i data0 = _mm256_i32gather_epi32((const int*)(src + (i + 0) * row_size), vindex, 1);
        __m256i data1 = _mm256_i32gather_epi32((const int*)(src + (i + 8) * row_size), vindex, 1);
        __m256i data2 = _mm256_i32gather_epi32((const int*)(src + (i + 16) * row_size), vindex, 1);
        __m256i data3 = _mm256_i32gather_epi32((const int*)(src + (i + 24) * row_size), vindex, 1);

        /* Mask to keep only lowest byte */
        __m256i mask = _mm256_set1_epi32(0x000000FF);
        data0 = _mm256_and_si256(data0, mask);
        data1 = _mm256_and_si256(data1, mask);
        data2 = _mm256_and_si256(data2, mask);
        data3 = _mm256_and_si256(data3, mask);

        /* Pack 32-bit -> 16-bit */
        __m256i packed01 = _mm256_packs_epi32(data0, data1);
        __m256i packed23 = _mm256_packs_epi32(data2, data3);

        /* Pack 16-bit -> 8-bit */
        __m256i packed = _mm256_packs_epi16(packed01, packed23);

        /* Fix ordering from packing */
        packed = _mm256_permute4x64_epi64(packed, 0xD8);  /* [0,2,1,3] */

        _mm256_storeu_si256((__m256i*)(signal_data + i), packed);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        signal_data[i] = src[i * row_size];
    }
}

/* ============================================================================
 * AVX512 IMPLEMENTATIONS (512-bit, available on newer CPUs)
 * ============================================================================ */

#ifdef __AVX512F__
/* AVX512: Extract 4-byte elements (16 samples at a time with gather) - OPTIMIZED */
static void extract_signal_4byte_avx512(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                        size_t sample_count, size_t signal_offset,
                                        size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 16) * 16;

    /* Create index vector for gather */
    __m512i vindex = _mm512_setr_epi32(
        0 * row_size, 1 * row_size, 2 * row_size, 3 * row_size,
        4 * row_size, 5 * row_size, 6 * row_size, 7 * row_size,
        8 * row_size, 9 * row_size, 10 * row_size, 11 * row_size,
        12 * row_size, 13 * row_size, 14 * row_size, 15 * row_size
    );

    for (size_t i = 0; i < simd_samples; i += 16) {
        /* Gather 16 int32 values from strided memory locations */
        /* Note: AVX512 gather parameter order is (index, base_ptr, scale) */
        __m512i data = _mm512_i32gather_epi32(vindex, (const void*)(src + i * row_size), 1);
        _mm512_storeu_si512(signal_data + i * 4, data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 4, src + i * row_size, 4);
    }
}

/* AVX512: Extract 8-byte elements (8 samples at a time with gather) - OPTIMIZED */
static void extract_signal_8byte_avx512(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                        size_t sample_count, size_t signal_offset,
                                        size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 8) * 8;

    /* Create index vector for gather */
    __m512i vindex = _mm512_setr_epi64(
        0 * row_size, 1 * row_size, 2 * row_size, 3 * row_size,
        4 * row_size, 5 * row_size, 6 * row_size, 7 * row_size
    );

    for (size_t i = 0; i < simd_samples; i += 8) {
        /* Gather 8 int64 values from strided memory locations */
        __m512i data = _mm512_i64gather_epi64(vindex, (const void*)(src + i * row_size), 1);
        _mm512_storeu_si512(signal_data + i * 8, data);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 8, src + i * row_size, 8);
    }
}

/* AVX512: Extract 2-byte elements (32 samples at a time) - OPTIMIZED */
static void extract_signal_2byte_avx512(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                        size_t sample_count, size_t signal_offset,
                                        size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 32) * 32;

    /* Optimized: Use mask and packs instead of packus for better performance */
    __m512i vindex = _mm512_setr_epi32(
        0 * row_size, 1 * row_size, 2 * row_size, 3 * row_size,
        4 * row_size, 5 * row_size, 6 * row_size, 7 * row_size,
        8 * row_size, 9 * row_size, 10 * row_size, 11 * row_size,
        12 * row_size, 13 * row_size, 14 * row_size, 15 * row_size
    );

    for (size_t i = 0; i < simd_samples; i += 32) {
        /* Gather 16-bit values as 32-bit */
        __m512i data_lo = _mm512_i32gather_epi32(vindex, (const void*)(src + i * row_size), 1);
        __m512i data_hi = _mm512_i32gather_epi32(vindex, (const void*)(src + (i + 16) * row_size), 1);

        /* Mask to keep only lower 16 bits */
        __m512i mask = _mm512_set1_epi32(0x0000FFFF);
        data_lo = _mm512_and_epi32(data_lo, mask);
        data_hi = _mm512_and_epi32(data_hi, mask);

        /* Pack 32-bit to 16-bit using packs */
        __m512i packed = _mm512_packs_epi32(data_lo, data_hi);

        /* Permute to correct order */
        packed = _mm512_permutexvar_epi64(
            _mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7), packed
        );

        _mm512_storeu_si512(signal_data + i * 2, packed);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        memcpy(signal_data + i * 2, src + i * row_size, 2);
    }
}

/* AVX512: Extract 1-byte elements (64 samples at a time) - NEW */
static void extract_signal_1byte_avx512(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                        size_t sample_count, size_t signal_offset,
                                        size_t row_size) {
    const uint8_t* src = row_data + signal_offset;
    size_t simd_samples = (sample_count / 64) * 64;

    __m512i vindex = _mm512_setr_epi32(
        0 * row_size, 1 * row_size, 2 * row_size, 3 * row_size,
        4 * row_size, 5 * row_size, 6 * row_size, 7 * row_size,
        8 * row_size, 9 * row_size, 10 * row_size, 11 * row_size,
        12 * row_size, 13 * row_size, 14 * row_size, 15 * row_size
    );

    for (size_t i = 0; i < simd_samples; i += 64) {
        /* Gather 64 bytes using four 16-element gathers */
        __m512i data0 = _mm512_i32gather_epi32(vindex, (const void*)(src + (i + 0) * row_size), 1);
        __m512i data1 = _mm512_i32gather_epi32(vindex, (const void*)(src + (i + 16) * row_size), 1);
        __m512i data2 = _mm512_i32gather_epi32(vindex, (const void*)(src + (i + 32) * row_size), 1);
        __m512i data3 = _mm512_i32gather_epi32(vindex, (const void*)(src + (i + 48) * row_size), 1);

        /* Mask to keep only lowest byte */
        __m512i mask = _mm512_set1_epi32(0x000000FF);
        data0 = _mm512_and_epi32(data0, mask);
        data1 = _mm512_and_epi32(data1, mask);
        data2 = _mm512_and_epi32(data2, mask);
        data3 = _mm512_and_epi32(data3, mask);

        /* Pack 32-bit -> 16-bit */
        __m512i packed01 = _mm512_packs_epi32(data0, data1);
        __m512i packed23 = _mm512_packs_epi32(data2, data3);

        /* Pack 16-bit -> 8-bit */
        __m512i packed = _mm512_packs_epi16(packed01, packed23);

        /* Fix ordering from packing */
        packed = _mm512_permutexvar_epi64(
            _mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7), packed
        );

        _mm512_storeu_si512(signal_data + i, packed);
    }

    /* Handle remaining samples with scalar */
    for (size_t i = simd_samples; i < sample_count; i++) {
        signal_data[i] = src[i * row_size];
    }
}
#endif /* __AVX512F__ */

/* Generic scalar extraction for other sizes */
static void extract_signal_scalar(const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                                  size_t sample_count, size_t signal_offset,
                                  size_t element_size, size_t row_size) {
    const uint8_t* src = row_data + signal_offset;

    for (size_t i = 0; i < sample_count; i++) {
        memcpy(signal_data + i * element_size, src + i * row_size, element_size);
    }
}

/* ============================================================================
 * EXTRACTION INFRASTRUCTURE
 * ============================================================================ */

/* Generic extraction that dispatches to best available SIMD implementation */
/* Non-static to allow usage in erg_batch_thread.c */
void extract_signal_generic(const ERG* erg, const uint8_t* restrict row_data, uint8_t* restrict signal_data,
                            size_t start_sample, size_t end_sample,
                            size_t signal_offset, size_t element_size, size_t row_size) {
    size_t         sample_count = end_sample - start_sample;
    const uint8_t* src          = row_data + start_sample * row_size;
    uint8_t*       dest         = signal_data + start_sample * element_size;

    /* Dispatch based on CPU capabilities and element size */
    /* Note: AVX-512 runtime detection works, but code requires compile-time support */
    /* If AVX-512 is detected but not compiled in, fall through to AVX2 */

#ifdef __AVX512F__
    if (erg->simd_level >= ERG_SIMD_AVX512) {
        switch (element_size) {
        case 1:
            extract_signal_1byte_avx512(src, dest, sample_count, signal_offset, row_size);
            return;
        case 2:
            extract_signal_2byte_avx512(src, dest, sample_count, signal_offset, row_size);
            return;
        case 4:
            extract_signal_4byte_avx512(src, dest, sample_count, signal_offset, row_size);
            return;
        case 8:
            extract_signal_8byte_avx512(src, dest, sample_count, signal_offset, row_size);
            return;
        }
    }
#endif

    if (erg->simd_level >= ERG_SIMD_AVX2) {
        switch (element_size) {
        case 1:
            extract_signal_1byte_avx2(src, dest, sample_count, signal_offset, row_size);
            return;
        case 2:
            extract_signal_2byte_avx2(src, dest, sample_count, signal_offset, row_size);
            return;
        case 4:
            extract_signal_4byte_avx2(src, dest, sample_count, signal_offset, row_size);
            return;
        case 8:
            extract_signal_8byte_avx2(src, dest, sample_count, signal_offset, row_size);
            return;
        }
    }

    if (erg->simd_level >= ERG_SIMD_SSE2) {
        switch (element_size) {
        case 1:
            extract_signal_1byte_sse2(src, dest, sample_count, signal_offset, row_size);
            return;
        case 2:
            extract_signal_2byte_sse2(src, dest, sample_count, signal_offset, row_size);
            return;
        case 4:
            extract_signal_4byte_sse2(src, dest, sample_count, signal_offset, row_size);
            return;
        case 8:
            extract_signal_8byte_sse2(src, dest, sample_count, signal_offset, row_size);
            return;
        }
    }

    /* Fallback to scalar */
    extract_signal_scalar(src, dest, sample_count, signal_offset, element_size, row_size);
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

    /* Extract raw signal data using SIMD */
    extract_signal_generic(erg, row_data, (uint8_t*)result, 0, erg->sample_count,
                          offset, sig->type_size, erg->row_size);

    /* Apply scaling if needed (factor != 1.0 or offset != 0.0) */
    if (sig->factor != 1.0 || sig->offset != 0.0) {
        /* Apply scaling in-place using native type */
        switch (sig->type) {
        case ERG_FLOAT: {
            float* data = (float*)result;
            float factor_f = (float)sig->factor;
            float offset_f = (float)sig->offset;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * factor_f + offset_f;
            }
            break;
        }
        case ERG_DOUBLE: {
            double* data = (double*)result;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * sig->factor + sig->offset;
            }
            break;
        }
        case ERG_INT: {
            int32_t* data = (int32_t*)result;
            int32_t factor_i = (int32_t)sig->factor;
            int32_t offset_i = (int32_t)sig->offset;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * factor_i + offset_i;
            }
            break;
        }
        case ERG_UINT: {
            uint32_t* data = (uint32_t*)result;
            uint32_t factor_u = (uint32_t)sig->factor;
            uint32_t offset_u = (uint32_t)sig->offset;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * factor_u + offset_u;
            }
            break;
        }
        case ERG_SHORT: {
            int16_t* data = (int16_t*)result;
            int16_t factor_s = (int16_t)sig->factor;
            int16_t offset_s = (int16_t)sig->offset;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * factor_s + offset_s;
            }
            break;
        }
        case ERG_USHORT: {
            uint16_t* data = (uint16_t*)result;
            uint16_t factor_us = (uint16_t)sig->factor;
            uint16_t offset_us = (uint16_t)sig->offset;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * factor_us + offset_us;
            }
            break;
        }
        case ERG_LONGLONG: {
            int64_t* data = (int64_t*)result;
            int64_t factor_ll = (int64_t)sig->factor;
            int64_t offset_ll = (int64_t)sig->offset;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * factor_ll + offset_ll;
            }
            break;
        }
        case ERG_ULONGLONG: {
            uint64_t* data = (uint64_t*)result;
            uint64_t factor_ull = (uint64_t)sig->factor;
            uint64_t offset_ull = (uint64_t)sig->offset;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * factor_ull + offset_ull;
            }
            break;
        }
        case ERG_CHAR: {
            int8_t* data = (int8_t*)result;
            int8_t factor_c = (int8_t)sig->factor;
            int8_t offset_c = (int8_t)sig->offset;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * factor_c + offset_c;
            }
            break;
        }
        case ERG_UCHAR: {
            uint8_t* data = (uint8_t*)result;
            uint8_t factor_uc = (uint8_t)sig->factor;
            uint8_t offset_uc = (uint8_t)sig->offset;
            for (size_t i = 0; i < erg->sample_count; i++) {
                data[i] = data[i] * factor_uc + offset_uc;
            }
            break;
        }
        case ERG_BYTES:
        default:
            /* No scaling for byte arrays */
            break;
        }
    }

    return result;
}

void erg_set_simd_level(ERG* erg, ERGSIMDLevel level) {
    if (erg) {
        erg->simd_level = level;
    }
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
