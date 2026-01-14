#include <stdio.h>
#include <stdlib.h>

#include "hash.h"

/* my own utility functions and string builder  */
static char *_str_arena(hm_arena *a, const char *s)
{
    size_t n=0, i=0;
    while(s[n++]!=0); // manual strlen

    char *p = arena_alloc(a, n);
    if (p) {
        while((p[i]=s[i])!='\0') i++; // manual memcpy
    }
    return p;
}

/* assume null-terminated - safe because not exposed to users */
static int s_cmp(const char *s1, const char *s2) {
    size_t n = 0;
    while (s1[n] && s2[n] && s1[n] == s2[n])
        n++;
    return (unsigned char)s1[n] - (unsigned char)s2[n];
}
#define match(s, n) s_cmp(s, n)==0

hm_arena arena_new(size_t cap) {
    hm_arena a;
    a.base = malloc(cap);
    a.cap  = cap;
    a.used = 0;
    return a;
}

void *arena_alloc(hm_arena *a, size_t sz) {
    size_t off = (a->used + 7) & ~7;
    if (off + sz > a->cap)
        return NULL;
    void *p = a->base + off;
    a->used = off + sz;
    return p;
}

void arena_free(hm_arena *a) {
    free(a->base);
    a->base = NULL;
    a->cap  = a->used = 0;
}

/**********/

#define FNV_OFFSET 0xcbf29ce484222325
#define FNV_PRIME  0x100000001b3

/* This returns a 64-bit (size_t) fnv-1a hash for a given key,
 * assumed to be NUL-terminated
 *
 * https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
 * */
static size_t hash_key(const char* key)
{
    size_t hash = FNV_OFFSET;
    for (const char* s = key; *s; s++){
        hash ^= (size_t)(unsigned char)(*s);
        hash *= FNV_PRIME;
    }
    return hash;
}

/* Linear probing needs to skip empty slots that WERE taken, but also we need
 * to be able to fill in removed spots - therefore we need a marker!
 * Create an empty string and declare TOMBSTONE to be a pointer to this string */
static char deleted[] = " ";
#define TOMBSTONE ((char*)deleted)

/* Checks if the map contains a map to key - will return on first empty spot
 * that is not same hash or tombstone */
int hm_contains_key(hashmap *hm, const char *key)
{
    if (hm->capacity == 0) return 0;
    /* cap-1 give a bitmask since we are powers of 2; '&' is basicly free,
     * while the modulos operation would be same answer, but more expensive
     * */
    size_t hash = hash_key(key);
    size_t hash_index = hash & (hm->capacity-1);

    hm_entry *e = &hm->items[hash_index];

    while (e->key) {
        if (e->key != TOMBSTONE && e->hash == hash)
                if(match(e->key, key))
                    return 1;
        hash_index = (hash_index + 1) & (hm->capacity-1);
        e = &hm->items[hash_index];
    }
    return 0;
}

/* Checks if the map contains one or more items->keys mapped to value. */
int hm_contains_value(hashmap *hm, uintptr_t value)
{
    for (size_t i = 0; i < hm->capacity; i++) {
        if (hm->items[i].key == NULL) continue;
        if (hm->items[i].key == TOMBSTONE) continue;
        if (hm->items[i].value == value) return 1;
    }
    return 0;
}

/* Helper to initiate arena */
static size_t _hm_set_arena(hashmap *hm)
{
    /*!!! --- magic number here for size --- !!!*/
    size_t size = 1024 * 1024 * 5;

    hm->arena = malloc(sizeof (hm_arena));
    if (!hm->arena) {
        printf("[ERROR] set arena\n");
        return -1;
    }

    *hm->arena = arena_new(size);
    if (!hm->arena->base) {
        printf("[ERROR] arena backing alloc failed\n");
        free(hm->arena);
        hm->arena = NULL;
        return -2;
    }
    return 0;
}

/* Internal helper to set an entry */
static int _hm_set_entry(hashmap *hm, const char *key, uintptr_t value)
{
    size_t hash = hash_key(key);
    size_t idx = hash & (hm->capacity - 1);

    hm_entry *items = hm->items;
    size_t tombstone_idx = (size_t)-1;

    for (;;) {
        hm_entry *e = &items[idx];

        if (e->key == NULL) {
            // empty slot -> insert (but prefer a tombstone if we saw one)
            if (tombstone_idx != (size_t)-1)
                e = &items[tombstone_idx];

            e->key   = _str_arena(hm->arena, key);
            e->value = value;
            e->hash  = hash;
            hm->count++;
            return 0;   // new insert
        }

        if (e->key == TOMBSTONE) {
            // record first tombstone, and keep probing
            if (tombstone_idx == (size_t)-1)
                tombstone_idx = idx;
        }
        else if (e->hash == hash && match(e->key, key)) {
            // found existing key -> overwrite
            e->value = value;
            return 1;  // overwrite
        }

        idx = (idx + 1) & (hm->capacity - 1);
    }
}

/* Reindexing when resizing */
static int _hm_resize(hashmap *hm)
{
    size_t old_cap = hm->capacity;
    size_t new_cap = hm->capacity << 1;

    if (!new_cap){
        new_cap = 256;      // just this once to init everything
    }

    hm_entry *old_items = hm->items;
    hm_entry *new_items = calloc(new_cap, sizeof *new_items);
    if(!new_items) return -1;

    hm->items = new_items;
    hm->capacity = new_cap;
    hm->count = 0; // will re count when reinserting

    /* With new capacity all the indexes is invalidated and needs to be
     * refreshed. Every entry needs a place according to their new index */
    for (size_t i = 0; i < old_cap; i++) {
        hm_entry *e = &old_items[i];
        if (!e->key || e->key == TOMBSTONE)
            continue;

        /* reinsert WITHOUT copying key or value */
        size_t idx = e->hash & (new_cap - 1);
        while (new_items[idx].key)
            idx = (idx + 1) & (new_cap - 1);

        new_items[idx] = *e;   /* copies key ptr, value, and hash */
        hm->count++;
    }

    free(old_items);
    return 0;
}

/* Creates a load factor of 70% making sure its a sweet spot for linear probing */
#define LOAD_FACTOR_NUM 7
#define LOAD_FACTOR_DEN 10

/* Inserts a key-value pair into the map. */
int hm_put(hashmap *hm, const char *key, uintptr_t value)
{
    if (!hm->arena) _hm_set_arena(hm);

    if (hm->count * LOAD_FACTOR_DEN >= hm->capacity * LOAD_FACTOR_NUM)
        _hm_resize(hm);

    return _hm_set_entry(hm, key, value);
}

/* Returns the value associated with key, or null */
int hm_get(hashmap *hm, const char *key)
{
    if (hm->capacity == 0) return 0;

    size_t hash = hash_key(key);
    size_t hash_index = hash & (hm->capacity-1);

    hm_entry *e = &hm->items[hash_index];

    while (e->key)
    {
        if (e->key != TOMBSTONE && e->hash == hash){
            if (match(e->key, key))
            {
                return (int)e->value;
            }
        }
        hash_index = (hash_index + 1) & (hm->capacity-1);
        e = &hm->items[hash_index];
    }
    return 0;
}

/* Removes the mapping for key */
int hm_remove(hashmap *hm, const char *key)
{
    if (hm->capacity == 0) return 0;

    size_t hash = hash_key(key);
    size_t hash_index = hash & (hm->capacity-1);

    hm_entry *e = &hm->items[hash_index];
    while (e->key)
    {
        if(e->hash == hash){
            if (match(e->key, key))
            {
                e->value = 0;

                e->key = TOMBSTONE;
                hm->count--;
                return 1;
                break;
            }
        }
        hash_index = (hash_index + 1) & (hm->capacity-1);
        e = &hm->items[hash_index];
    }
    return 0;
}
/* Frees arena and then the items array, freeing everything */
void hm_destroy(hashmap *hm)
{
    arena_free(hm->arena);
    free(hm->arena);
    free(hm->items);
}
