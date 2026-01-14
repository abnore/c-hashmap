// heavy_test.c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../hash.h"

#define ASSERT(c) do { \
    if (!(c)) { \
        fprintf(stderr, "ASSERT FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
        exit(1); \
    } \
} while (0)

// Stress parameters
#define KEYS   200000
#define OPS    1000000
#define KEYLEN 32

typedef struct {
    int live;   // 1 = present, 0 = absent
    int value;  // only meaningful when live==1 (non-zero)
} Shadow;

// key = "k<id>"
static void make_key(char *buf, size_t n, int id) {
    snprintf(buf, n, "k%d", id);
}

static int rand_nonzero(void) {
    int v;
    do {
        v = rand();
    } while (v == 0);
    return v;
}

int main(void) {
    srand((unsigned)time(NULL));

    hashmap hm = (hashmap){0};

    Shadow *sh = calloc(KEYS, sizeof *sh);
    ASSERT(sh);

    // All keys start "not live"
    for (int i = 0; i < KEYS; i++) {
        sh[i].live  = 0;
        sh[i].value = 0;
    }

    char keybuf[KEYLEN];

    for (int op = 0; op < OPS; op++) {
        int id = rand() % KEYS;
        Shadow *s = &sh[id];
        int was_live = s->live;

        int r = rand() % 100;

        if (r < 40) {
            // PUT
            make_key(keybuf, sizeof keybuf, id);
            int v = rand_nonzero();

            int existed_before = hm_contains_key(&hm, keybuf);
            int ret = hm_put(&hm, keybuf, v);
            ASSERT(ret == (existed_before ? 1 : 0));

            s->live  = 1;
            s->value = v;
        } else if (r < 70) {
            // GET
            make_key(keybuf, sizeof keybuf, id);
            int got = hm_get(&hm, keybuf);

            if (s->live) {
                ASSERT(got == s->value);
            } else {
                ASSERT(got == 0);
            }
        } else {
            // REMOVE
            make_key(keybuf, sizeof keybuf, id);
            int ret = hm_remove(&hm, keybuf);

            // 1 iff it was live
            ASSERT(ret == (was_live ? 1 : 0));

            s->live  = 0;
            s->value = 0;
        }

        // Periodic audit of random keys
        if ((op & 0xFFFF) == 0) {
            for (int j = 0; j < 1000; j++) {
                int k = rand() % KEYS;
                Shadow *x = &sh[k];

                make_key(keybuf, sizeof keybuf, k);
                int got = hm_get(&hm, keybuf);
                int ck  = hm_contains_key(&hm, keybuf);

                if (x->live) {
                    ASSERT(got == x->value);
                    ASSERT(ck == 1);
                } else {
                    ASSERT(got == 0);
                    ASSERT(ck == 0);
                }
            }
        }
    }

    // Full sweep: verify all keys against shadow
    for (int id = 0; id < KEYS; id++) {
        Shadow *s = &sh[id];
        make_key(keybuf, sizeof keybuf, id);

        int got = hm_get(&hm, keybuf);
        int ck  = hm_contains_key(&hm, keybuf);

        if (s->live) {
            ASSERT(got == s->value);
            ASSERT(ck == 1);
        } else {
            ASSERT(got == 0);
            ASSERT(ck == 0);
        }
    }

    // Remove everything
    for (int id = 0; id < KEYS; id++) {
        Shadow *s = &sh[id];
        make_key(keybuf, sizeof keybuf, id);

        int was_live = s->live;
        int ret = hm_remove(&hm, keybuf);
        ASSERT(ret == (was_live ? 1 : 0));

        s->live  = 0;
        s->value = 0;
    }

    // Map must be empty now
    for (int id = 0; id < KEYS; id++) {
        make_key(keybuf, sizeof keybuf, id);
        ASSERT(hm_get(&hm, keybuf) == 0);
        ASSERT(hm_contains_key(&hm, keybuf) == 0);
    }

    free(sh);
    hm_destroy(&hm);

    printf("ALL HEAVY TESTS PASSED\n");
    return 0;
}
