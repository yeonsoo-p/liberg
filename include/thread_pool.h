#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Thread pool for reusable worker threads
 * Eliminates thread creation/destruction overhead for repeated tasks
 */
typedef struct ThreadPool ThreadPool;

/**
 * Work function signature for thread pool tasks
 * @param arg User-provided argument
 * @return Result pointer (optional, can be NULL)
 */
typedef void* (*ThreadPoolWorkFunc)(void* arg);

/**
 * Create a thread pool with specified number of worker threads
 * Threads are created immediately and wait for work
 *
 * @param num_threads Number of worker threads to create
 * @return Pointer to thread pool, or NULL on failure
 */
ThreadPool* thread_pool_create(int num_threads);

/**
 * Submit work to the thread pool
 * Work items are distributed among available threads
 *
 * @param pool Thread pool
 * @param work_func Function to execute
 * @param args Array of arguments (one per thread)
 * @param num_work_items Number of work items (should equal num_threads)
 */
void thread_pool_submit(ThreadPool* pool, ThreadPoolWorkFunc work_func,
                       void** args, int num_work_items);

/**
 * Wait for all submitted work to complete
 * Blocks until all threads finish their current work
 *
 * @param pool Thread pool
 */
void thread_pool_wait(ThreadPool* pool);

/**
 * Get number of threads in the pool
 *
 * @param pool Thread pool
 * @return Number of threads
 */
int thread_pool_get_thread_count(ThreadPool* pool);

/**
 * Destroy thread pool and free all resources
 * Waits for any pending work to complete first
 *
 * @param pool Thread pool to destroy
 */
void thread_pool_destroy(ThreadPool* pool);

#ifdef __cplusplus
}
#endif

#endif /* THREAD_POOL_H */
