#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>

typedef struct __jobqueue jobqueue_t;

typedef struct __threadpool {
    size_t count;
    pthread_t *workers;
    jobqueue_t *jobqueue;
} * tpool_t;

typedef struct __arg {
    int *arr;
    int left_len;
    int right_len;
    sem_t *sem;
} arg;

/**
 * Create a thread pool containing specified number of threads.
 * If successful, the thread pool is returned. Otherwise, it
 * returns NULL.
 */
tpool_t tpool_create(size_t count);

/**
 * Schedules the specific function to be executed.
 * If successful, a future object representing the execution of
 * the task is returned. Otherwise, it returns NULL.
 */
void *tpool_apply(tpool_t pool, void *(*func)(arg *), arg *arg);

/**
 * Wait for all pending tasks to complete before destroying the thread pool.
 */
int tpool_join(tpool_t pool);
