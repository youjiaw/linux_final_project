#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

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

#define MAX_THREADS 32
#define MAX_SEGMENTS 20

void merge_sort_part(int *arr, const int len)
{
    if (len == 1)
        return;

    int pending[MAX_SEGMENTS];
    int pending_count = 0, total_length = 0;

    int init_size = 1;
    for (int i = 0; i < len; i += init_size) {
        // adding an item
        pending[pending_count++] = init_size;
        total_length += init_size;

        // keep merging segments with the same length
        while (pending_count >= 2 &&
               pending[pending_count - 1] == pending[pending_count - 2]) {
            int merge_length = pending[pending_count - 1];
            int left_start = total_length - 2 * merge_length;

            merge(arr + left_start, merge_length, merge_length);

            // update the length of the last segment
            pending_count -= 1;
            pending[pending_count - 1] = 2 * merge_length;
        }
    }

    // merge the remaining segments
    while (pending_count > 1) {
        int right_length = pending[pending_count - 1];
        int left_length = pending[pending_count - 2];
        int left_start = total_length - right_length - left_length;

        merge(arr + left_start, left_length, right_length);

        pending_count -= 1;
        pending[pending_count - 1] = left_length + right_length;
    }
}

typedef struct {
    int *arr;
    int len;
} merge_args_t;

static void *merge_thread(void *args)
{
    merge_args_t *m_args = (merge_args_t *) args;
    merge_sort_part(m_args->arr, m_args->len);
    return NULL;
}

void merge_sort(int *arr, const int len)
{
    pthread_t threads[MAX_THREADS];
    merge_args_t args[MAX_THREADS];

    int arr_len = len / MAX_THREADS,
        final_len = len - arr_len * (MAX_THREADS - 1);
    for (int i = 0; i < MAX_THREADS; i++) {
        if (i == MAX_THREADS - 1)
            arr_len = final_len;

        args[i] = (merge_args_t){arr + i * arr_len, arr_len};
        pthread_create(&threads[i], NULL, merge_thread, &args[i]);
    }

    for (int i = 0; i < MAX_THREADS; i++)
        pthread_join(threads[i], NULL);

    // merge the sorted parts from 'right to left'
    // because in merge function, the left part is copied to a new array
    for (int i = MAX_THREADS - 1; i > 0; i--)
        merge(arr + (i - 1) * arr_len, arr_len, len - i * arr_len);
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
