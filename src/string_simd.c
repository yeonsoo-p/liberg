#include <immintrin.h> // AVX2 intrinsics
#include <stddef.h>
#include <stdint.h>

/**
 * SIMD-optimized strlen using AVX2
 * Processes 32 bytes at a time to find null terminator
 *
 * Uses three-phase approach:
 * 1. Alignment loop: Process bytes until aligned OR null found (scalar)
 * 2. Main loop: Process aligned 32-byte chunks (fast aligned loads)
 * 3. Safe throughout: Uses unaligned loads to avoid page boundary issues
 */
size_t strlen_simd(const char* str) {
    const char* s = str;
    __m256i zero = _mm256_setzero_si256();

    /* Phase 1: Handle unaligned start - process bytes until 32-byte aligned
     * IMPORTANT: Check for null terminator to avoid reading past string end */
    uintptr_t addr         = (uintptr_t)s;
    size_t    misalignment = addr & 31;

    if (misalignment != 0) {
        /* Process bytes one at a time until aligned OR null found */
        while (((uintptr_t)s & 31) != 0) {
            if (*s == '\0') {
                return s - str;
            }
            s++;
        }
    }

    /* Phase 2: Main loop - s is now 32-byte aligned
     * Use aligned loads for better performance, but we still use unaligned
     * loads to be safe near page boundaries */
    while (1) {
        /* Use unaligned load to be safe - compiler/CPU will optimize if aligned
         * This prevents segfaults when crossing page boundaries */
        __m256i chunk = _mm256_loadu_si256((const __m256i*)s);

        /* Compare with zero */
        __m256i cmp = _mm256_cmpeq_epi8(chunk, zero);

        /* Create mask from comparison */
        uint32_t mask = _mm256_movemask_epi8(cmp);

        if (mask != 0) {
            /* Found a zero byte - find which one */
            int pos = __builtin_ctz(mask); // Count trailing zeros
            return (s - str) + pos;
        }

        s += 32;
    }
}

/**
 * SIMD-optimized memcpy using AVX2
 * Processes 32 bytes at a time for bulk copying
 *
 * Note: For small sizes (<= 64 bytes), falls back to simple copy
 * to avoid AVX2 overhead
 */
void* memcpy_simd(void* dest, const void* src, size_t n) {
    char*       d = (char*)dest;
    const char* s = (const char*)src;

    /* For small copies, use simple byte copy (faster due to less overhead) */
    if (n <= 64) {
        char* d_end = d + n;
        while (d < d_end) {
            *d++ = *s++;
        }
        return dest;
    }

    /* Handle unaligned start */
    size_t misalignment = (uintptr_t)d & 31;
    if (misalignment != 0) {
        size_t to_align = 32 - misalignment;
        if (to_align > n)
            to_align = n;

        for (size_t i = 0; i < to_align; i++) {
            *d++ = *s++;
        }
        n -= to_align;
    }

    /* Process 32 bytes at a time with AVX2 */
    while (n >= 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)s);
        _mm256_store_si256((__m256i*)d, chunk);

        d += 32;
        s += 32;
        n -= 32;
    }

    /* Handle remaining bytes */
    while (n > 0) {
        *d++ = *s++;
        n--;
    }

    return dest;
}

/**
 * SIMD-optimized memcpy for unaligned destinations
 * Uses unaligned stores to avoid alignment overhead
 * Better for arena allocations which may not be aligned
 *
 * Optimized for small strings (common case in parsing)
 */
void* memcpy_simd_unaligned(void* dest, const void* src, size_t n) {
    char*       d = (char*)dest;
    const char* s = (const char*)src;

    /* For very small copies, use direct assignment (fastest) */
    if (n <= 16) {
        /* Unrolled loop for small sizes - overlapping copy trick */
        if (n >= 8) {
            *((uint64_t*)d)           = *((uint64_t*)s);
            *((uint64_t*)(d + n - 8)) = *((uint64_t*)(s + n - 8));
        } else if (n >= 4) {
            *((uint32_t*)d)           = *((uint32_t*)s);
            *((uint32_t*)(d + n - 4)) = *((uint32_t*)(s + n - 4));
        } else if (n >= 2) {
            *((uint16_t*)d)           = *((uint16_t*)s);
            *((uint16_t*)(d + n - 2)) = *((uint16_t*)(s + n - 2));
        } else if (n == 1) {
            *d = *s;
        }
        return dest;
    }

    /* For medium copies, use simple loop (still faster than AVX2 overhead) */
    if (n <= 64) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
        return dest;
    }

    /* For large copies, use AVX2 */
    while (n >= 32) {
        __m256i chunk = _mm256_loadu_si256((const __m256i*)s);
        _mm256_storeu_si256((__m256i*)d, chunk);

        d += 32;
        s += 32;
        n -= 32;
    }

    /* Handle remaining bytes (0-31 bytes left)
     * Use scalar copy for safety - trying to use SIMD with overlapping copy
     * would read out of bounds when n < 32 (e.g., n=20 would read from s-12) */
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}
