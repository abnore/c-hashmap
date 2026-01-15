#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../hash.h"

/* basic put/get/remove */
static void test_basic(void) {
    hashmap hm = (hashmap){0};

    assert(hm_put(&hm, "a", 10) == 0);
    assert(hm_contains_key(&hm, "a") == 1);
    assert(hm_get(&hm, "a") == 10);

    assert(hm_remove(&hm, "a") == 1);
    assert(hm_contains_key(&hm, "a") == 0);
    assert(hm_get(&hm, "a") == 0);

    hm_destroy(&hm);
}

/* overwrite same key keeps count == 1 and value updated */
static void test_overwrite(void) {
    hashmap hm = (hashmap){0};

    assert(hm_put(&hm, "k", 1) == 0);
    assert(hm_get(&hm, "k") == 1);

    assert(hm_put(&hm, "k", 2) == 1);
    assert(hm_get(&hm, "k") == 2);
    assert(hm_contains_key(&hm, "k") == 1);
    assert(hm.count == 1);

    hm_destroy(&hm);
}

/* lookups/removes of missing keys on a non-empty map */
static void test_missing_key_nonempty(void) {
    hashmap hm = (hashmap){0};

    assert(hm_put(&hm, "init", 42) == 0);

    assert(hm_contains_key(&hm, "nope") == 0);
    assert(hm_get(&hm, "nope") == 0);
    assert(hm_remove(&hm, "nope") == 0);

    assert(hm_contains_key(&hm, "init") == 1);
    assert(hm_get(&hm, "init") == 42);

    hm_destroy(&hm);
}

/* inserting enough keys to force at least one resize */
static void test_resize(void) {
    hashmap hm = (hashmap){0};

    const int N = 500;
    char key[32];

    for (int i = 0; i < N; i++) {
        sprintf(key, "k%d", i);
        assert(hm_put(&hm, key, (uintptr_t)i) == 0);
    }

    for (int i = 0; i < N; i++) {
        sprintf(key, "k%d", i);
        assert(hm_contains_key(&hm, key) == 1);
        assert(hm_get(&hm, key) == i);
    }

    hm_destroy(&hm);
}

/* basic tombstone behavior */
static void test_tombstone_basic(void) {
    hashmap hm = (hashmap){0};

    hm_put(&hm, "x", 100);
    hm_put(&hm, "y", 200);

    assert(hm_contains_key(&hm, "x") == 1);
    assert(hm_contains_key(&hm, "y") == 1);

    assert(hm_remove(&hm, "x") == 1);
    assert(hm_contains_key(&hm, "x") == 0);
    assert(hm_get(&hm, "x") == 0);

    assert(hm_contains_key(&hm, "y") == 1);
    assert(hm_get(&hm, "y") == 200);

    hm_destroy(&hm);
}

/* reuse of tombstones */
static void test_tombstone_reuse(void) {
    hashmap hm = (hashmap){0};

    hm_put(&hm, "a", 1);
    hm_put(&hm, "b", 2);

    assert(hm_remove(&hm, "a") == 1);

    hm_put(&hm, "c", 3);
    assert(hm_contains_key(&hm, "c") == 1);
    assert(hm_get(&hm, "c") == 3);

    hm_destroy(&hm);
}

/* scan values */
static void test_contains_value(void) {
    hashmap hm = (hashmap){0};

    hm_put(&hm, "a", 111);
    hm_put(&hm, "b", 222);

    assert(hm_contains_value(&hm, 111) == 1);
    assert(hm_contains_value(&hm, 222) == 1);
    assert(hm_contains_value(&hm, 333) == 0);

    hm_destroy(&hm);
}

/* remove twice = OK */
static void test_double_remove(void) {
    hashmap hm = (hashmap){0};

    hm_put(&hm, "x", 1);
    assert(hm_remove(&hm, "x") == 1);
    assert(hm_remove(&hm, "x") == 0);

    hm_destroy(&hm);
}

/* mixed put/remove */
static void test_mixed_put_remove(void) {
    hashmap hm = (hashmap){0};
    char key[32];
    const int N = 200;

    for (int i = 0; i < N; i++) {
        sprintf(key, "k%d", i);
        hm_put(&hm, key, i);
    }

    for (int i = 0; i < N; i += 3) {
        sprintf(key, "k%d", i);
        assert(hm_remove(&hm, key) == 1);
    }

    for (int i = 0; i < N; i++) {
        sprintf(key, "k%d", i);
        if (i % 3 == 0) {
            assert(hm_contains_key(&hm, key) == 0);
        } else {
            assert(hm_contains_key(&hm, key) == 1);
            assert(hm_get(&hm, key) == i);
        }
    }

    hm_destroy(&hm);
}

/* arena lazily created + grows to multiple slabs */
static void test_arena_usage(void) {
    hashmap hm = (hashmap){0};
    char key[32];

    const int N = 50000;
    for (int i = 0; i < N; i++) {
        sprintf(key, "k%d", i);
        hm_put(&hm, key, (uintptr_t)i);
    }

    assert(hm.count == (size_t)N);
    assert(hm.arena != NULL);
    assert(hm.arena->head != NULL);

    hm_destroy(&hm);
}

int main(void) {
    test_basic();
    test_overwrite();
    test_missing_key_nonempty();
    test_resize();
    test_tombstone_basic();
    test_tombstone_reuse();
    test_contains_value();
    test_double_remove();
    test_mixed_put_remove();
    test_arena_usage();
    printf("ALL TESTS PASSED\n");
    return 0;
}
