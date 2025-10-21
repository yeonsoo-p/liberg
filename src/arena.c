#include <arena.h>
#include <string_simd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Internal structure for arena chunks */
typedef struct ArenaChunk
{
    struct ArenaChunk *next; /* Next chunk in chain */
    size_t capacity;         /* Total capacity of this chunk */
    size_t used;             /* Bytes used in this chunk */
    char data[];             /* Flexible array member for actual data */
} ArenaChunk;

/* Allocate a new chunk */
static ArenaChunk *chunk_create(size_t capacity)
{
    ArenaChunk *chunk = (ArenaChunk *)malloc(sizeof(ArenaChunk) + capacity);
    if (!chunk)
    {
        fprintf(stderr, "FATAL: Failed to allocate arena chunk of %zu bytes\n", capacity);
        exit(1);
    }
    chunk->next = NULL;
    chunk->capacity = capacity;
    chunk->used = 0;
    return chunk;
}

void arena_init(Arena *arena, size_t initial_size)
{
    arena->chunk_size = initial_size;
    arena->first = chunk_create(initial_size);
    arena->current = arena->first;
}

void arena_reserve(Arena *arena, size_t total_needed)
{
    /* Calculate how much we already have */
    size_t total_available = 0;
    ArenaChunk *chunk = arena->first;
    while (chunk)
    {
        total_available += chunk->capacity;
        chunk = chunk->next;
    }

    /* Allocate additional chunks if needed */
    while (total_available < total_needed)
    {
        size_t chunk_size = arena->chunk_size;
        /* Double chunk size for next allocation */
        if (arena->chunk_size < 16 * 1024 * 1024)
        { /* Cap at 16MB per chunk */
            arena->chunk_size *= 2;
        }

        ArenaChunk *new_chunk = chunk_create(chunk_size);

        /* Add to end of chain */
        ArenaChunk *last = arena->first;
        while (last->next)
        {
            last = last->next;
        }
        last->next = new_chunk;

        total_available += chunk_size;
    }
}

char *arena_alloc(Arena *arena, size_t size)
{
    /* Check if current chunk has enough space */
    if (arena->current->used + size <= arena->current->capacity)
    {
        char *ptr = arena->current->data + arena->current->used;
        arena->current->used += size;
        return ptr;
    }

    /* Try to find a chunk with enough space */
    ArenaChunk *chunk = arena->current->next;
    while (chunk)
    {
        if (chunk->capacity - chunk->used >= size)
        {
            arena->current = chunk;
            char *ptr = chunk->data + chunk->used;
            chunk->used += size;
            return ptr;
        }
        chunk = chunk->next;
    }

    /* No existing chunk has space - allocate a new one */
    size_t new_chunk_size = arena->chunk_size;

    /* If requested size is larger than default chunk size, make chunk bigger */
    if (size > new_chunk_size)
    {
        new_chunk_size = size * 2; /* Give some headroom */
    }

    ArenaChunk *new_chunk = chunk_create(new_chunk_size);

    /* Add to end of chain */
    ArenaChunk *last = arena->first;
    while (last->next)
    {
        last = last->next;
    }
    last->next = new_chunk;

    /* Update chunk size for next allocation (exponential growth) */
    if (arena->chunk_size < 16 * 1024 * 1024)
    { /* Cap at 16MB */
        arena->chunk_size *= 2;
    }

    /* Allocate from new chunk */
    arena->current = new_chunk;
    char *ptr = new_chunk->data;
    new_chunk->used = size;
    return ptr;
}

char *arena_strdup(Arena *arena, const char *str)
{
    if (!str)
    {
        fprintf(stderr, "FATAL: arena_strdup called with NULL string\n");
        exit(1);
    }

    size_t len = strlen_simd(str) + 1;
    char *ptr = arena_alloc(arena, len);
    memcpy_simd_unaligned(ptr, str, len);
    return ptr;
}

char *arena_strndup(Arena *arena, const char *str, size_t n)
{
    if (!str)
    {
        fprintf(stderr, "FATAL: arena_strndup called with NULL string\n");
        exit(1);
    }

    char *ptr = arena_alloc(arena, n + 1);
    memcpy_simd_unaligned(ptr, str, n);
    ptr[n] = '\0';
    return ptr;
}

void arena_reset(Arena *arena)
{
    /* Reset usage in all chunks */
    ArenaChunk *chunk = arena->first;
    while (chunk)
    {
        chunk->used = 0;
        chunk = chunk->next;
    }
    arena->current = arena->first;
}

void arena_free(Arena *arena)
{
    /* Free all chunks */
    ArenaChunk *chunk = arena->first;
    while (chunk)
    {
        ArenaChunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }
    arena->first = NULL;
    arena->current = NULL;
    arena->chunk_size = 0;
}

size_t arena_get_used(const Arena *arena)
{
    size_t total = 0;
    ArenaChunk *chunk = arena->first;
    while (chunk)
    {
        total += chunk->used;
        chunk = chunk->next;
    }
    return total;
}

size_t arena_get_capacity(const Arena *arena)
{
    size_t total = 0;
    ArenaChunk *chunk = arena->first;
    while (chunk)
    {
        total += chunk->capacity;
        chunk = chunk->next;
    }
    return total;
}
