/* Test thread pool basic functionality */
#include <stdio.h>
#include <stdlib.h>
#include "../include/thread_pool.h"

/* Simple work function */
void* simple_work(void* arg) {
    int* value = (int*)arg;
    (*value)++;
    return NULL;
}

int main(void) {
    printf("Testing thread pool with multiple create/destroy cycles...\n\n");

    int thread_counts[] = {1, 2, 4, 8};
    int num_configs = 4;

    for (int config = 0; config < num_configs; config++) {
        int num_threads = thread_counts[config];

        printf("Creating pool with %d thread(s)...\n", num_threads);
        ThreadPool* pool = num_threads > 1 ? thread_pool_create(num_threads) : NULL;

        if (pool) {
            printf("  Pool created successfully\n");

            /* Submit work multiple times to test reuse (like the benchmark does) */
            int iterations = 10;
            printf("  Running %d iterations...\n", iterations);

            for (int iter = 0; iter < iterations; iter++) {
                int* values = (int*)calloc(num_threads, sizeof(int));
                void** args = (void**)malloc(num_threads * sizeof(void*));

                for (int i = 0; i < num_threads; i++) {
                    args[i] = &values[i];
                }

                thread_pool_submit(pool, simple_work, args, num_threads);
                thread_pool_wait(pool);

                free(values);
                free(args);
            }

            printf("  All %d iterations completed successfully\n", iterations);

            printf("  Destroying pool...\n");
            thread_pool_destroy(pool);
            printf("  Pool destroyed\n");
        } else {
            printf("  Single-threaded mode (no pool)\n");
        }

        printf("\n");
    }

    printf("All tests completed successfully!\n");
    return 0;
}
