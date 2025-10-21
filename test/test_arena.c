#include <arena.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * Test program for the shared arena allocator
 * Verifies cross-platform functionality
 */

int main(void) {
    printf("=== Arena Allocator Test ===\n\n");

    /* Test 1: Basic initialization */
    printf("Test 1: Basic initialization...\n");
    Arena arena;
    arena_init(&arena, 1024);
    assert(arena_get_capacity(&arena) == 1024);
    assert(arena_get_used(&arena) == 0);
    printf("  ✓ Arena initialized with 1024 bytes\n\n");

    /* Test 2: Simple allocation */
    printf("Test 2: Simple allocation...\n");
    char *ptr1 = arena_alloc(&arena, 100);
    assert(ptr1 != NULL);
    assert(arena_get_used(&arena) == 100);
    memset(ptr1, 'A', 100);
    printf("  ✓ Allocated 100 bytes\n");
    printf("  ✓ Arena usage: %zu / %zu bytes\n\n", arena_get_used(&arena), arena_get_capacity(&arena));

    /* Test 3: Multiple allocations */
    printf("Test 3: Multiple allocations...\n");
    char *ptr2 = arena_alloc(&arena, 200);
    char *ptr3 = arena_alloc(&arena, 150);
    assert(ptr2 != NULL && ptr3 != NULL);
    assert(arena_get_used(&arena) == 450);
    printf("  ✓ Three allocations totaling 450 bytes\n");
    printf("  ✓ Arena usage: %zu / %zu bytes\n\n", arena_get_used(&arena), arena_get_capacity(&arena));

    /* Test 4: String duplication */
    printf("Test 4: String duplication...\n");
    const char *test_str = "Hello, Arena!";
    char *dup_str = arena_strdup(&arena, test_str);
    assert(dup_str != NULL);
    assert(strcmp(dup_str, test_str) == 0);
    printf("  ✓ String duplicated: \"%s\"\n", dup_str);
    printf("  ✓ Arena usage: %zu / %zu bytes\n\n", arena_get_used(&arena), arena_get_capacity(&arena));

    /* Test 5: Partial string duplication */
    printf("Test 5: Partial string duplication...\n");
    const char *long_str = "This is a long string";
    char *partial = arena_strndup(&arena, long_str, 7);
    assert(partial != NULL);
    assert(strcmp(partial, "This is") == 0);
    printf("  ✓ Partial string copied: \"%s\"\n", partial);
    printf("  ✓ Arena usage: %zu / %zu bytes\n\n", arena_get_used(&arena), arena_get_capacity(&arena));

    /* Test 6: Arena growth (now adds new chunks) */
    printf("Test 6: Arena automatic growth...\n");
    size_t old_capacity = arena_get_capacity(&arena);
    char *large = arena_alloc(&arena, 2048);  /* Exceeds initial 1024 capacity */
    assert(large != NULL);
    assert(arena_get_capacity(&arena) > old_capacity);
    printf("  ✓ Arena grew from %zu to %zu bytes\n", old_capacity, arena_get_capacity(&arena));
    printf("  ✓ Large allocation successful\n\n");

    /* Test 7: Verify data integrity after growth */
    printf("Test 7: Data integrity after growth...\n");
    assert(strcmp(dup_str, test_str) == 0);
    assert(strcmp(partial, "This is") == 0);
    printf("  ✓ Previously allocated strings still valid\n");
    printf("  ✓ Data safe with chunk-based arena (no pointer invalidation!)\n\n");

    /* Test 8: Reset arena */
    printf("Test 8: Arena reset...\n");
    size_t capacity_before_reset = arena_get_capacity(&arena);
    arena_reset(&arena);
    assert(arena_get_used(&arena) == 0);
    assert(arena_get_capacity(&arena) == capacity_before_reset);
    printf("  ✓ Arena reset (used = 0, capacity = %zu)\n", arena_get_capacity(&arena));
    printf("  ✓ Memory not freed, ready for reuse\n\n");

    /* Test 9: Reuse after reset */
    printf("Test 9: Reuse after reset...\n");
    char *new_str = arena_strdup(&arena, "Reused arena");
    assert(new_str != NULL);
    assert(strcmp(new_str, "Reused arena") == 0);
    printf("  ✓ Arena successfully reused after reset\n");
    printf("  ✓ New string: \"%s\"\n\n", new_str);

    /* Test 10: Get usage statistics */
    printf("Test 10: Usage statistics...\n");
    size_t used = arena_get_used(&arena);
    size_t capacity = arena_get_capacity(&arena);
    printf("  ✓ Used: %zu bytes\n", used);
    printf("  ✓ Capacity: %zu bytes\n", capacity);
    printf("  ✓ Utilization: %.1f%%\n\n", (used * 100.0) / capacity);

    /* Cleanup */
    arena_free(&arena);
    printf("Test 11: Cleanup...\n");
    printf("  ✓ Arena freed\n\n");

    printf("=== All tests passed! ===\n");
    printf("Arena allocator is working correctly on this platform.\n");

    return 0;
}
