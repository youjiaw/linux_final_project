#include "tpool.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct __threadtask {
    void *(*func)(arg *);
    struct __arg *arg;
    struct __threadtask *next;
} threadtask_t;

typedef struct __jobqueue {
    threadtask_t *head, *tail;
    pthread_cond_t cond_nonempty;
    pthread_mutex_t rwlock;
} jobqueue_t;

static jobqueue_t *jobqueue_create(void)
{
    jobqueue_t *jobqueue = malloc(sizeof(jobqueue_t));
    if (jobqueue) {
        jobqueue->head = jobqueue->tail = NULL;
        pthread_cond_init(&jobqueue->cond_nonempty, NULL);
        pthread_mutex_init(&jobqueue->rwlock, NULL);
    }
    return jobqueue;
}

static void jobqueue_destroy(jobqueue_t *jobqueue)
{
    threadtask_t *tmp = jobqueue->head;
    while (tmp) {
        jobqueue->head = jobqueue->head->next;
        free(tmp);
        tmp = jobqueue->head;
    }

    pthread_mutex_destroy(&jobqueue->rwlock);
    pthread_cond_destroy(&jobqueue->cond_nonempty);
    free(jobqueue);
}

static void __jobqueue_fetch_cleanup(void *arg)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *) arg;
    pthread_mutex_unlock(mutex);
}

static void *jobqueue_fetch(void *queue)
{
    jobqueue_t *jobqueue = (jobqueue_t *) queue;
    threadtask_t *task;
    int old_state;

    pthread_cleanup_push(__jobqueue_fetch_cleanup, (void *) &jobqueue->rwlock);

    while (1) {
        pthread_mutex_lock(&jobqueue->rwlock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);
        pthread_testcancel();

        while (!jobqueue->tail)
            pthread_cond_wait(&jobqueue->cond_nonempty, &jobqueue->rwlock);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
        if (jobqueue->head == jobqueue->tail) {
            task = jobqueue->tail;
            jobqueue->head = jobqueue->tail = NULL;
        } else {
            threadtask_t *tmp;
            for (tmp = jobqueue->head; tmp->next != jobqueue->tail;
                 tmp = tmp->next)
                ;
            task = tmp->next;
            tmp->next = NULL;
            jobqueue->tail = tmp;
        }
        pthread_mutex_unlock(&jobqueue->rwlock);

        if (task->func) {
            void *ret_value = task->func(task->arg);
            free(task);
        } else {
            free(task);
            break;
        }
    }

    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}

struct __threadpool *tpool_create(size_t count)
{
    jobqueue_t *jobqueue = jobqueue_create();
    struct __threadpool *pool = malloc(sizeof(struct __threadpool));
    if (!jobqueue || !pool) {
        if (jobqueue)
            jobqueue_destroy(jobqueue);
        free(pool);
        return NULL;
    }

    pool->count = count, pool->jobqueue = jobqueue;
    if ((pool->workers = malloc(count * sizeof(pthread_t)))) {
        for (int i = 0; i < count; i++) {
            if (pthread_create(&pool->workers[i], NULL, jobqueue_fetch,
                               (void *) jobqueue)) {
                for (int j = 0; j < i; j++)
                    pthread_cancel(pool->workers[j]);
                for (int j = 0; j < i; j++)
                    pthread_join(pool->workers[j], NULL);
                free(pool->workers);
                jobqueue_destroy(jobqueue);
                free(pool);
                return NULL;
            }
        }
        return pool;
    }

    jobqueue_destroy(jobqueue);
    free(pool);
    return NULL;
}

void *tpool_apply(struct __threadpool *pool, void *(*func)(arg *), arg *arg)
{
    jobqueue_t *jobqueue = pool->jobqueue;
    threadtask_t *new_head = malloc(sizeof(threadtask_t));

    if (new_head) {
        new_head->func = func, new_head->arg = arg;
        pthread_mutex_lock(&jobqueue->rwlock);
        if (jobqueue->head) {
            new_head->next = jobqueue->head;
            jobqueue->head = new_head;
        } else {
            jobqueue->head = jobqueue->tail = new_head;
            pthread_cond_broadcast(&jobqueue->cond_nonempty);
        }
        pthread_mutex_unlock(&jobqueue->rwlock);
    } else if (new_head) {
        free(new_head);
        return NULL;
    }
}

int tpool_join(struct __threadpool *pool)
{
    size_t num_threads = pool->count;
    for (int i = 0; i < num_threads; i++)
        tpool_apply(pool, NULL, NULL);
    for (int i = 0; i < num_threads; i++)
        pthread_join(pool->workers[i], NULL);
    free(pool->workers);
    jobqueue_destroy(pool->jobqueue);
    free(pool);
    return 0;
}
