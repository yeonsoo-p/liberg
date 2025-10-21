#include <infofile.h>
#include <infofile_arena.h>
#include <infofile_simd.h>
#include <infofile_arena_simd.h>
#include <infofile_arena_simd_opt.h>
#include <infofile_simple.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ITERATIONS 10

typedef struct
{
    double parse_time_ms;
    double free_time_ms;
    double total_time_ms;
    size_t entry_count;
    size_t memory_allocated;
} BenchmarkResult;

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void benchmark_standard(const char *filename, int iterations, BenchmarkResult *result)
{
    double start, parse_end, free_end;
    double total_parse = 0.0;
    double total_free = 0.0;

    for (int i = 0; i < iterations; i++)
    {
        InfoFile info;
        infofile_init(&info);

        start = get_time_ms();
        if (infofile_parse_file(filename, &info) != 0)
        {
            fprintf(stderr, "Parse failed\n");
            return;
        }
        parse_end = get_time_ms();

        if (i == 0)
        {
            result->entry_count = info.count;

            // Estimate memory usage: entries array + each key/value string
            size_t mem = info.capacity * sizeof(InfoFileEntry);
            for (size_t j = 0; j < info.count; j++)
            {
                mem += strlen(info.entries[j].key) + 1;    // key string + null
                mem += strlen(info.entries[j].value) + 1;  // value string + null
            }
            result->memory_allocated = mem;
        }

        infofile_free(&info);
        free_end = get_time_ms();

        total_parse += (parse_end - start);
        total_free += (free_end - parse_end);
    }

    result->parse_time_ms = total_parse / iterations;
    result->free_time_ms = total_free / iterations;
    result->total_time_ms = result->parse_time_ms + result->free_time_ms;
}

static void benchmark_arena(const char *filename, int iterations, BenchmarkResult *result)
{
    double start, parse_end, free_end;
    double total_parse = 0.0;
    double total_free = 0.0;

    for (int i = 0; i < iterations; i++)
    {
        InfoFileArena info;
        infofile_arena_init(&info);

        start = get_time_ms();
        if (infofile_arena_parse_file(filename, &info) != 0)
        {
            fprintf(stderr, "Parse failed\n");
            return;
        }
        parse_end = get_time_ms();

        if (i == 0)
        {
            result->entry_count = info.count;
            result->memory_allocated = arena_get_used(&info.arena);
        }

        infofile_arena_free(&info);
        free_end = get_time_ms();

        total_parse += (parse_end - start);
        total_free += (free_end - parse_end);
    }

    result->parse_time_ms = total_parse / iterations;
    result->free_time_ms = total_free / iterations;
    result->total_time_ms = result->parse_time_ms + result->free_time_ms;
}

static void print_results(const char *name, const BenchmarkResult *result)
{
    printf("\n%s:\n", name);
    printf("  Entries:       %zu\n", result->entry_count);
    printf("  Parse time:    %.3f ms\n", result->parse_time_ms);
    printf("  Free time:     %.3f ms\n", result->free_time_ms);
    printf("  Total time:    %.3f ms\n", result->total_time_ms);
    if (result->memory_allocated > 0)
    {
        printf("  Memory used:   %zu bytes (%.1f KB)\n",
               result->memory_allocated,
               result->memory_allocated / 1024.0);
    }
}

static void benchmark_simd(const char *filename, int iterations, BenchmarkResult *result)
{
    double start, parse_end, free_end;
    double total_parse = 0.0;
    double total_free = 0.0;

    for (int i = 0; i < iterations; i++)
    {
        InfoFileSIMD info;
        infofile_simd_init(&info);

        start = get_time_ms();
        if (infofile_simd_parse_file(filename, &info) != 0)
        {
            fprintf(stderr, "Parse failed\n");
            return;
        }
        parse_end = get_time_ms();

        if (i == 0)
        {
            result->entry_count = info.count;

            // Estimate memory usage: entries array + each key/value string
            size_t mem = info.capacity * sizeof(InfoFileEntrySIMD);
            for (size_t j = 0; j < info.count; j++)
            {
                mem += strlen(info.entries[j].key) + 1;    // key string + null
                mem += strlen(info.entries[j].value) + 1;  // value string + null
            }
            result->memory_allocated = mem;
        }

        infofile_simd_free(&info);
        free_end = get_time_ms();

        total_parse += (parse_end - start);
        total_free += (free_end - parse_end);
    }

    result->parse_time_ms = total_parse / iterations;
    result->free_time_ms = total_free / iterations;
    result->total_time_ms = result->parse_time_ms + result->free_time_ms;
}

static void benchmark_arena_simd(const char *filename, int iterations, BenchmarkResult *result)
{
    double start, parse_end, free_end;
    double total_parse = 0.0;
    double total_free = 0.0;

    for (int i = 0; i < iterations; i++)
    {
        InfoFileArenaSimd info;
        infofile_arena_simd_init(&info);

        start = get_time_ms();
        if (infofile_arena_simd_parse_file(filename, &info) != 0)
        {
            fprintf(stderr, "Parse failed\n");
            return;
        }
        parse_end = get_time_ms();

        if (i == 0)
        {
            result->entry_count = info.count;
            result->memory_allocated = arena_get_used(&info.arena);
        }

        infofile_arena_simd_free(&info);
        free_end = get_time_ms();

        total_parse += (parse_end - start);
        total_free += (free_end - parse_end);
    }

    result->parse_time_ms = total_parse / iterations;
    result->free_time_ms = total_free / iterations;
    result->total_time_ms = result->parse_time_ms + result->free_time_ms;
}

static void benchmark_arena_simd_opt(const char *filename, int iterations, BenchmarkResult *result)
{
    double start, parse_end, free_end;
    double total_parse = 0.0;
    double total_free = 0.0;

    for (int i = 0; i < iterations; i++)
    {
        InfoFileArenaSimdOpt info;
        infofile_arena_simd_opt_init(&info);

        start = get_time_ms();
        if (infofile_arena_simd_opt_parse_file(filename, &info) != 0)
        {
            fprintf(stderr, "Parse failed\n");
            return;
        }
        parse_end = get_time_ms();

        if (i == 0)
        {
            result->entry_count = info.count;
            result->memory_allocated = arena_get_used(&info.arena.key_arena) + arena_get_used(&info.arena.value_arena);
        }

        infofile_arena_simd_opt_free(&info);
        free_end = get_time_ms();

        total_parse += (parse_end - start);
        total_free += (free_end - parse_end);
    }

    result->parse_time_ms = total_parse / iterations;
    result->free_time_ms = total_free / iterations;
    result->total_time_ms = result->parse_time_ms + result->free_time_ms;
}

static void benchmark_simple(const char *filename, int iterations, BenchmarkResult *result)
{
    double start, parse_end, free_end;
    double total_parse = 0.0;
    double total_free = 0.0;

    for (int i = 0; i < iterations; i++)
    {
        InfoFileSimple info;
        infofile_simple_init(&info);

        start = get_time_ms();
        if (infofile_simple_parse_file(filename, &info) != 0)
        {
            fprintf(stderr, "Parse failed\n");
            return;
        }
        parse_end = get_time_ms();

        if (i == 0)
        {
            result->entry_count = info.count;
            result->memory_allocated = 0;  // TODO: Track memory for simple parser
        }

        infofile_simple_free(&info);
        free_end = get_time_ms();

        total_parse += (parse_end - start);
        total_free += (free_end - parse_end);
    }

    result->parse_time_ms = total_parse / iterations;
    result->free_time_ms = total_free / iterations;
    result->total_time_ms = result->parse_time_ms + result->free_time_ms;
}

static void print_comparison(const BenchmarkResult *standard, const BenchmarkResult *arena, const BenchmarkResult *simd, const BenchmarkResult *arena_simd, const BenchmarkResult *arena_simd_opt, const BenchmarkResult *simple)
{
    printf("\n=== Performance Comparison ===\n");

    double arena_parse_speedup = standard->parse_time_ms / arena->parse_time_ms;
    double arena_total_speedup = standard->total_time_ms / arena->total_time_ms;

    double simd_parse_speedup = standard->parse_time_ms / simd->parse_time_ms;
    double simd_total_speedup = standard->total_time_ms / simd->total_time_ms;

    double arena_simd_parse_speedup = standard->parse_time_ms / arena_simd->parse_time_ms;
    double arena_simd_total_speedup = standard->total_time_ms / arena_simd->total_time_ms;

    double arena_simd_opt_parse_speedup = standard->parse_time_ms / arena_simd_opt->parse_time_ms;
    double arena_simd_opt_total_speedup = standard->total_time_ms / arena_simd_opt->total_time_ms;

    double simple_parse_speedup = standard->parse_time_ms / simple->parse_time_ms;
    double simple_total_speedup = standard->total_time_ms / simple->total_time_ms;

    printf("\nSpeedup vs Standard:\n");
    printf("                       Parse          Total\n");
    printf("  Arena:               %.2fx          %.2fx\n",
           arena_parse_speedup, arena_total_speedup);
    printf("  SIMD:                %.2fx          %.2fx\n",
           simd_parse_speedup, simd_total_speedup);
    printf("  Arena+SIMD:          %.2fx          %.2fx\n",
           arena_simd_parse_speedup, arena_simd_total_speedup);
    printf("  Arena+SIMD+Opt:      %.2fx          %.2fx  ⭐\n",
           arena_simd_opt_parse_speedup, arena_simd_opt_total_speedup);
    printf("  Simple:              %.2fx          %.2fx\n",
           simple_parse_speedup, simple_total_speedup);

    printf("\nOptimized vs Previous Best (Arena+SIMD):\n");
    double opt_improvement_parse = arena_simd->parse_time_ms / arena_simd_opt->parse_time_ms;
    double opt_improvement_total = arena_simd->total_time_ms / arena_simd_opt->total_time_ms;
    printf("  Parse improvement:   %.2fx faster (%.1f ms saved)\n",
           opt_improvement_parse,
           arena_simd->parse_time_ms - arena_simd_opt->parse_time_ms);
    printf("  Total improvement:   %.2fx faster (%.1f ms saved)\n",
           opt_improvement_total,
           arena_simd->total_time_ms - arena_simd_opt->total_time_ms);

    if (standard->memory_allocated > 0)
    {
        printf("\nMemory Comparison:\n");
        printf("  Standard:            %zu bytes (%.1f MB)\n",
               standard->memory_allocated,
               standard->memory_allocated / 1024.0 / 1024.0);
        printf("  Arena:               %zu bytes (%.1f MB)\n",
               arena->memory_allocated,
               arena->memory_allocated / 1024.0 / 1024.0);
        printf("  SIMD:                %zu bytes (%.1f MB)\n",
               simd->memory_allocated,
               simd->memory_allocated / 1024.0 / 1024.0);
        printf("  Arena+SIMD:          %zu bytes (%.1f MB)\n",
               arena_simd->memory_allocated,
               arena_simd->memory_allocated / 1024.0 / 1024.0);
        printf("  Arena+SIMD+Opt:      %zu bytes (%.1f MB)\n",
               arena_simd_opt->memory_allocated,
               arena_simd_opt->memory_allocated / 1024.0 / 1024.0);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <info file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int iterations = ITERATIONS;

    printf("=== Info File Parser Benchmark ===\n");
    printf("File:       %s\n", filename);
    printf("Iterations: %d\n", iterations);

    BenchmarkResult standard_result = {0};
    BenchmarkResult arena_result = {0};
    BenchmarkResult simd_result = {0};
    BenchmarkResult arena_simd_result = {0};
    BenchmarkResult arena_simd_opt_result = {0};
    BenchmarkResult simple_result = {0};

    printf("\nRunning standard allocator benchmark...\n");
    benchmark_standard(filename, iterations, &standard_result);
    print_results("Standard Allocator", &standard_result);

    printf("\nRunning arena allocator benchmark...\n");
    benchmark_arena(filename, iterations, &arena_result);
    print_results("Arena Allocator", &arena_result);

    printf("\nRunning SIMD optimized benchmark...\n");
    benchmark_simd(filename, iterations, &simd_result);
    print_results("SIMD Optimized", &simd_result);

    printf("\nRunning Arena+SIMD optimized benchmark...\n");
    benchmark_arena_simd(filename, iterations, &arena_simd_result);
    print_results("Arena+SIMD Optimized", &arena_simd_result);

    printf("\nRunning Arena+SIMD Fully Optimized benchmark...\n");
    benchmark_arena_simd_opt(filename, iterations, &arena_simd_opt_result);
    print_results("Arena+SIMD Fully Optimized", &arena_simd_opt_result);

    printf("\nRunning Simple parser benchmark...\n");
    benchmark_simple(filename, iterations, &simple_result);
    print_results("Simple Parser", &simple_result);

    print_comparison(&standard_result, &arena_result, &simd_result, &arena_simd_result, &arena_simd_opt_result, &simple_result);

    printf("\n=== Benchmark Complete ===\n");

    return 0;
}
