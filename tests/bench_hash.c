#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include "hash.h"

static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec*1000000000LL + ts.tv_nsec;
}

int main(void) {
    const size_t N = 50000;
    char buf[64];
    hashmap hm = {0};

    // Warmup: allocate arena + bucket table
    hm_put(&hm, "warmup", 123);
    hm_remove(&hm, "warmup");

    long long start, end;

    // INSERT
    start = now_ns();
    for (size_t i=0; i<N; i++) {
        sprintf(buf, "k%zu", i);    // or sprintf
        hm_put(&hm, buf, i);
    }
    end = now_ns();
    double ins_ms = (end-start)/1e6;
    printf("Insert:  %zu ops in %.2f ms = %.1f Mops/sec\n",
           N, ins_ms, (N / (ins_ms/1000.0)) / 1e6);

    // LOOKUP
    size_t hits=0;
    start = now_ns();
    for (size_t i=0; i<N; i++) {
        sprintf(buf, "k%zu", i);
        if (hm_get(&hm, buf) == i) hits++;
    }
    end = now_ns();
    double get_ms = (end-start)/1e6;
    printf("Lookup:  %zu ops in %.2f ms = %.1f Mops/sec (%zu hits)\n",
           N, get_ms, (N / (get_ms/1000.0)) / 1e6, hits);

    // REMOVE
    size_t removed = 0;
    start = now_ns();
    for (size_t i=0; i<N; i++) {
        sprintf(buf, "k%zu", i);
        if (hm_remove(&hm, buf))
            removed++;
    }
    end = now_ns();
    double rm_ms = (end-start)/1e6;
    printf("Remove:  %zu ops in %.2f ms = %.1f Mops/sec (%zu removed)\n",
           N, rm_ms, (N / (rm_ms/1000.0)) / 1e6, removed);

    hm_destroy(&hm);
    return 0;
}
