#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "tpool.h"

#include <semaphore.h>

static void merge(int *arr, const int left_len, const int right_len)
{
    int *left_copy = calloc(left_len, sizeof(int));
    memcpy(left_copy, arr, left_len * sizeof(int));
    int left_idx = 0, right_idx = 0, cur_idx = 0;

    while (left_idx < left_len && right_idx < right_len) {
        if (left_copy[left_idx] <= arr[left_len + right_idx]) {
            arr[cur_idx++] = left_copy[left_idx++];
        } else {
            arr[cur_idx++] = arr[left_len + right_idx++];
        }
    }

    while (left_idx < left_len)
        arr[cur_idx++] = left_copy[left_idx++];

    free(left_copy);
}

static int thread_count = 0;


void *merge_thread(arg *m_args)
{
    merge(m_args->arr, m_args->left_len, m_args->right_len);
    sem_post(m_args->sem);
    free(m_args);
    return NULL;
}

void merge_sort(int *arr, const int len)
{
    tpool_t pool = tpool_create(32);
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        exit(1);
    }

    for (int curr_size = 1; curr_size < len; curr_size = curr_size * 2) {
        sem_t level_done;
        sem_init(&level_done, 0, 0);
        int tasks = 0;

        for (int left_start = 0; left_start < len;
             left_start += curr_size * 2) {
            int right_start = left_start + curr_size;
            if (right_start >= len)
                continue;

            int left_len = curr_size;
            int right_len = (right_start + curr_size <= len)
                                ? curr_size
                                : (len - right_start);

            arg *task_arg = malloc(sizeof(arg));
            if (!task_arg) {
                fprintf(stderr,
                        "Failed to allocate memory for task argument\n");
                exit(1);
            }
            task_arg->arr = arr + left_start;
            task_arg->left_len = left_len;
            task_arg->right_len = right_len;
            task_arg->sem = &level_done;

            tpool_apply(pool, merge_thread, task_arg);

            tasks++;
        }

        for (int i = 0; i < tasks; i++)
            sem_wait(&level_done);
        sem_destroy(&level_done);
    }
    tpool_join(pool);
}

typedef struct {
    uint32_t a, b, c, d;
} rand_context_t;

/* See https://burtleburtle.net/bob/rand/smallprng.html */
#define ROT(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
uint32_t rand_next(rand_context_t *x)
{
    uint32_t e = x->a - ROT(x->b, 27);
    x->a = x->b ^ ROT(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

void rand_init(rand_context_t *x, uint32_t seed)
{
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (size_t i = 0; i < 20; ++i)
        (void) rand_next(x);
}

int iabs(int n)
{
    int mask = n >> 31;
    return (mask & -n) | (~mask & n);
}

#define N_ITEMS 1000000

int main(int argc, char **argv)
{
    rand_context_t r;
    rand_init(&r, (uintptr_t) &main ^ getpid());

    /* shared by forked processes */
    int *arr = mmap(NULL, N_ITEMS * sizeof(int), PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, 0, 0);

    for (int i = 0; i < N_ITEMS; ++i)
        arr[i] = iabs((int) rand_next(&r));

    merge_sort(arr, N_ITEMS);

    for (int i = 1; i < N_ITEMS; ++i) {
        if (arr[i] < arr[i - 1]) {
            fprintf(stderr, "Ascending order is expected.\n");
            exit(1);
        }
    }
    printf("OK!\n");

    return 0;
}
