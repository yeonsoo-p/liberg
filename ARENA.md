# Arena Allocator

## Overview

The liberg project now uses a shared, cross-platform arena allocator (`arena.h` / `arena.c`) that is used across all parser implementations. This eliminates code duplication and ensures consistent memory management behavior across Windows and Linux.

## What is an Arena Allocator?

An arena allocator is a memory management technique that:
- Pre-allocates a large block of memory
- Serves allocation requests by bumping a pointer (O(1) time)
- Frees all allocations at once by discarding the entire arena
- Grows automatically when needed (via `realloc`)
- **Exits on allocation failure** (fail-fast for reliability)

### Benefits

1. **Performance**: O(1) allocation (vs malloc's variable time)
2. **No per-allocation overhead**: No bookkeeping metadata for each allocation
3. **Cache locality**: Related data stored contiguously
4. **Simple cleanup**: Free entire arena at once
5. **No fragmentation**: Linear allocation pattern
6. **Fail-fast**: Immediate, clear errors instead of silent failures

### Trade-offs

- Cannot free individual allocations
- Must free entire arena at once
- Growing the arena (via realloc) can invalidate existing pointers
- Exits on allocation failure (not suitable for graceful degradation scenarios)

## API Reference

### Types

```c
typedef struct {
    char *buffer;       /* Memory buffer */
    size_t capacity;    /* Total capacity in bytes */
    size_t used;        /* Bytes currently used */
} Arena;
```

### Core Functions

```c
/* Initialize arena with specified capacity
 * Exits on allocation failure */
void arena_init(Arena *arena, size_t initial_size);

/* Ensure arena has at least the specified capacity (may trigger realloc)
 * Exits on allocation failure */
void arena_ensure_capacity(Arena *arena, size_t total_needed);

/* Allocate memory from arena
 * Automatically grows arena if needed
 * Exits on allocation failure
 * Returns: Pointer to allocated memory (never NULL) */
char *arena_alloc(Arena *arena, size_t size);

/* Duplicate a string in the arena
 * Exits on allocation failure or if str is NULL
 * Returns: Pointer to duplicated string (never NULL) */
char *arena_strdup(Arena *arena, const char *str);

/* Duplicate up to n characters of a string
 * Exits on allocation failure or if str is NULL
 * Returns: Pointer to duplicated string (never NULL, null-terminated) */
char *arena_strndup(Arena *arena, const char *str, size_t n);

/* Reset arena (mark all memory as available, don't free) */
void arena_reset(Arena *arena);

/* Free all memory */
void arena_free(Arena *arena);
```

### Error Handling

**All allocation functions follow a fail-fast approach:**

- On allocation failure (malloc/realloc returns NULL), the arena functions print a descriptive error message to stderr and call `exit(1)`
- This ensures that allocation failures are immediately visible and debuggable
- Returned pointers are **never NULL** - if you get a pointer back, the allocation succeeded
- NULL string arguments to `arena_strdup`/`arena_strndup` are treated as errors

**Example error messages:**
```
FATAL: arena_init failed to allocate 1024 bytes
FATAL: arena_grow failed to realloc from 1024 to 2048 bytes (needed 1500 more)
FATAL: arena_strdup called with NULL string
FATAL: arena_ensure_capacity called on uninitialized arena
```

This design choice is appropriate for parsers where:
- Memory allocation failure indicates a catastrophic system state
- Silent failures would lead to difficult-to-debug crashes later
- Clear, immediate error messages are more valuable than graceful degradation

### Utility Functions

```c
/* Get current usage */
size_t arena_get_used(const Arena *arena);

/* Get total capacity */
size_t arena_get_capacity(const Arena *arena);
```

## Usage Pattern

### Basic Usage

```c
#include <arena.h>

Arena arena;
arena_init(&arena, 1024);  // 1KB initial size

// Allocate memory
char *data = arena_alloc(&arena, 100);

// Duplicate strings
char *str = arena_strdup(&arena, "Hello");

// Free everything at once
arena_free(&arena);
```

### Pre-sizing for Performance

For best performance, pre-size the arena to avoid reallocation during parsing:

```c
// Estimate needed size (e.g., 2x file size)
size_t estimated = file_size * 2;
arena_ensure_capacity(&arena, estimated);

// Now parse without reallocation
// All pointers remain valid
```

### Dual Arena Layout

The optimized parser uses separate arenas for hot (keys) and cold (values) data:

```c
typedef struct {
    Arena key_arena;    /* Frequently accessed */
    Arena value_arena;  /* Less frequently accessed */
} DualArena;
```

This improves cache locality during key lookups.

## Integration with Parsers

All arena-based parsers now use the shared implementation:

| Parser | Arena Usage |
|--------|-------------|
| `infofile_arena.c` | Single `Arena` |
| `infofile_arena_simd.c` | Single `Arena` |
| `infofile_arena_simd_opt.c` | `DualArena` (keys/values separate) |

### Migration Notes

The old parser-specific arena implementations (`ArenaSimd`, `ArenaSimdOpt`) have been replaced with the shared `Arena` type. The behavior is identical, but the code is now centralized.

## Platform Compatibility

The arena allocator is fully cross-platform:

- **Linux**: Tested on Linux 6.14.0-33-generic with GCC 14.2.0
- **Windows**: Uses standard C library functions (malloc/realloc/free)
- **macOS**: Compatible (same as Linux)

No platform-specific code is required. The implementation uses only standard C11 functions.

## Testing

Run the arena allocator test:

```bash
./build/test_arena
```

This tests:
- Initialization
- Allocation
- String duplication
- Automatic growth
- Data integrity after reallocation
- Reset and reuse
- Memory cleanup

## Performance Impact

The shared arena allocator has **zero performance impact**. Benchmarks show identical performance before and after the refactoring:

- **Arena+SIMD+Opt**: 36.2ms parse time (3.00x speedup vs baseline)
- Memory usage: 56.3 MB
- All 6 parsers pass 21 test cases

## Files

### Core Implementation
- `include/arena.h` - Public API
- `src/arena.c` - Implementation

### Parser Headers (updated to use shared arena)
- `include/infofile_arena.h`
- `include/infofile_arena_simd.h`
- `include/infofile_arena_simd_opt.h`

### Tests
- `test/test_arena.c` - Standalone arena allocator tests

## Future Enhancements

Potential improvements:
1. **Thread-local arenas**: One arena per thread for parallel parsing
2. **Memory pooling**: Reuse arenas across multiple parse operations
3. **Alignment control**: Support aligned allocations for SIMD
4. **Statistics**: Track allocation patterns for optimization

## License

Same as the liberg project (refer to root LICENSE file).
