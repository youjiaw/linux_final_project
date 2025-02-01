#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static int *merge(const int *left_half,
                  const int left_len,
                  const int *right_half,
                  const int right_len)
{
    int *merged = calloc(left_len + right_len, sizeof(int));
    int left_idx = 0, right_idx = 0, cur_idx = 0;

    while (left_idx < left_len && right_idx < right_len)
    {
        if (left_half[left_idx] <= right_half[right_idx])
        {
            merged[cur_idx++] = left_half[left_idx++];
        }
        else
        {
            merged[cur_idx++] = right_half[right_idx++];
        }
    }

    while (left_idx < left_len)
        merged[cur_idx++] = left_half[left_idx++];

    while (right_idx < right_len)
        merged[cur_idx++] = right_half[right_idx++];

    return merged;
}

static int fork_count = 0;

void merge_sort(int *arr, const int len)
{
    if (len == 1)
        return;

    const int mid = len / 2;
    const int left_len = len - mid;
    const int right_len = mid;

    /* If forked too often, it gets way too slow. */
    if (fork_count < 5)
    {
        pid_t pid = fork();
        fork_count++;
        if (pid == 0)
        { /* Child process */
            merge_sort(arr, left_len);
            exit(0);
        }

        /* Parent process */
        merge_sort(arr + left_len, right_len);
        waitpid(pid, NULL, 0);
    }
    else
    {
        merge_sort(arr, left_len);
        merge_sort(arr + left_len, right_len);
    }

    memcpy(arr, merge(arr, left_len, arr + left_len, right_len),
           len * sizeof(int));
}

typedef struct
{
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
        (void)rand_next(x);
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
    rand_init(&r, (uintptr_t)&main ^ getpid());

    /* shared by forked processes */
    int *arr = mmap(NULL, N_ITEMS * sizeof(int), PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, 0, 0);

    for (int i = 0; i < N_ITEMS; ++i)
        arr[i] = iabs((int)rand_next(&r));

    merge_sort(arr, N_ITEMS);

    for (int i = 1; i < N_ITEMS; ++i)
    {
        if (arr[i] < arr[i - 1])
        {
            fprintf(stderr, "Ascending order is expected.\n");
            exit(1);
        }
    }
    printf("OK!\n");

    return 0;
}
