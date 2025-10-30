#include <thread_pool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

/* Thread ID wrapper for passing to worker threads */
typedef struct {
    ThreadPool* pool;
    int thread_id;
} ThreadContext;

/* Thread pool implementation */
struct ThreadPool {
    int num_threads;

#ifdef _WIN32
    HANDLE* threads;
    HANDLE* work_events;     /* Events to signal work availability */
    HANDLE  complete_event;  /* Event to signal work completion */
    CRITICAL_SECTION lock;
    ThreadContext* contexts; /* Thread contexts for passing IDs */
#else
    pthread_t* threads;
    pthread_mutex_t lock;
    pthread_cond_t work_cond;
    pthread_cond_t complete_cond;
    ThreadContext* contexts; /* Thread contexts for passing IDs */
#endif

    ThreadPoolWorkFunc work_func;
    void** work_args;
    int num_work_items;

    volatile int active_threads;
    volatile int shutdown;
};

#ifdef _WIN32
static unsigned int __stdcall worker_thread(void* arg) {
    ThreadContext* ctx = (ThreadContext*)arg;
    ThreadPool* pool = ctx->pool;
    int thread_id = ctx->thread_id;

    while (1) {
        /* Wait for work */
        WaitForSingleObject(pool->work_events[thread_id], INFINITE);

        if (pool->shutdown) {
            break;
        }

        /* Execute work */
        if (pool->work_func && thread_id < pool->num_work_items) {
            pool->work_func(pool->work_args[thread_id]);

            /* Signal completion */
            EnterCriticalSection(&pool->lock);
            pool->active_threads--;
            if (pool->active_threads == 0) {
                SetEvent(pool->complete_event);
            }
            LeaveCriticalSection(&pool->lock);
        }
    }

    return 0;
}
#else
static void* worker_thread(void* arg) {
    ThreadContext* ctx = (ThreadContext*)arg;
    ThreadPool* pool = ctx->pool;
    int thread_id = ctx->thread_id;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        /* Wait for work (work_func will be set when work is submitted) */
        while (pool->work_func == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->work_cond, &pool->lock);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        /* Grab work function and args */
        ThreadPoolWorkFunc func = pool->work_func;
        void* arg_for_thread = thread_id < pool->num_work_items ? pool->work_args[thread_id] : NULL;

        pthread_mutex_unlock(&pool->lock);

        /* Execute work */
        if (func && arg_for_thread) {
            func(arg_for_thread);
        }

        /* Signal completion */
        pthread_mutex_lock(&pool->lock);
        pool->active_threads--;
        if (pool->active_threads == 0) {
            pool->work_func = NULL;  /* Clear work func to prepare for next submission */
            pthread_cond_signal(&pool->complete_cond);
        }
        pthread_mutex_unlock(&pool->lock);
    }

    return NULL;
}
#endif

ThreadPool* thread_pool_create(int num_threads) {
    if (num_threads <= 0) {
        return NULL;
    }

    ThreadPool* pool = (ThreadPool*)calloc(1, sizeof(ThreadPool));
    if (!pool) {
        return NULL;
    }

    pool->num_threads = num_threads;
    pool->shutdown = 0;
    pool->active_threads = 0;

#ifdef _WIN32
    InitializeCriticalSection(&pool->lock);
    pool->complete_event = CreateEvent(NULL, TRUE, FALSE, NULL);  /* Manual-reset event */

    pool->threads = (HANDLE*)malloc(num_threads * sizeof(HANDLE));
    pool->work_events = (HANDLE*)malloc(num_threads * sizeof(HANDLE));
    pool->contexts = (ThreadContext*)malloc(num_threads * sizeof(ThreadContext));

    for (int i = 0; i < num_threads; i++) {
        pool->work_events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        pool->contexts[i].pool = pool;
        pool->contexts[i].thread_id = i;
        pool->threads[i] = (HANDLE)_beginthreadex(NULL, 0, worker_thread, &pool->contexts[i], 0, NULL);
    }
#else
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->work_cond, NULL);
    pthread_cond_init(&pool->complete_cond, NULL);

    pool->threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }
#endif

    return pool;
}

void thread_pool_submit(ThreadPool* pool, ThreadPoolWorkFunc work_func,
                       void** args, int num_work_items) {
    if (!pool || !work_func) {
        return;
    }

#ifdef _WIN32
    EnterCriticalSection(&pool->lock);

    /* Reset the completion event for this new batch of work */
    ResetEvent(pool->complete_event);

    pool->work_func = work_func;
    pool->work_args = args;
    pool->num_work_items = num_work_items;
    pool->active_threads = num_work_items < pool->num_threads ? num_work_items : pool->num_threads;

    /* Capture the number of threads to signal before leaving the lock */
    int threads_to_signal = pool->active_threads;

    LeaveCriticalSection(&pool->lock);

    /* Signal all threads to start work */
    for (int i = 0; i < threads_to_signal; i++) {
        SetEvent(pool->work_events[i]);
    }
#else
    pthread_mutex_lock(&pool->lock);

    pool->work_func = work_func;
    pool->work_args = args;
    pool->num_work_items = num_work_items;
    pool->active_threads = num_work_items < pool->num_threads ? num_work_items : pool->num_threads;

    pthread_cond_broadcast(&pool->work_cond);
    pthread_mutex_unlock(&pool->lock);
#endif
}

void thread_pool_wait(ThreadPool* pool) {
    if (!pool) {
        return;
    }

#ifdef _WIN32
    WaitForSingleObject(pool->complete_event, INFINITE);

    /* Clear work function to prepare for next submission */
    EnterCriticalSection(&pool->lock);
    pool->work_func = NULL;
    LeaveCriticalSection(&pool->lock);
#else
    pthread_mutex_lock(&pool->lock);
    while (pool->active_threads > 0) {
        pthread_cond_wait(&pool->complete_cond, &pool->lock);
    }
    pool->work_func = NULL;  /* Clear work func to prepare for next submission */
    pthread_mutex_unlock(&pool->lock);
#endif
}

int thread_pool_get_thread_count(ThreadPool* pool) {
    return pool ? pool->num_threads : 0;
}

void thread_pool_destroy(ThreadPool* pool) {
    if (!pool) {
        return;
    }

    /* Signal shutdown */
#ifdef _WIN32
    EnterCriticalSection(&pool->lock);
    pool->shutdown = 1;
    LeaveCriticalSection(&pool->lock);

    /* Wake all threads */
    for (int i = 0; i < pool->num_threads; i++) {
        SetEvent(pool->work_events[i]);
    }

    /* Wait for threads to finish */
    WaitForMultipleObjects(pool->num_threads, pool->threads, TRUE, INFINITE);

    /* Cleanup */
    for (int i = 0; i < pool->num_threads; i++) {
        CloseHandle(pool->threads[i]);
        CloseHandle(pool->work_events[i]);
    }
    CloseHandle(pool->complete_event);

    free(pool->threads);
    free(pool->work_events);
    free(pool->contexts);

    DeleteCriticalSection(&pool->lock);
#else
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->work_cond);
    pthread_mutex_unlock(&pool->lock);

    /* Wait for threads to finish */
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    free(pool->threads);

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->work_cond);
    pthread_cond_destroy(&pool->complete_cond);
#endif

    free(pool);
}
