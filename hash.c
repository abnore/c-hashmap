#include "hash.h"

/* --- initial size for hash table slots and arena chunks (in bytes) --- */
#define HM_INITIAL_CAPACITY 1u<<9   // 512 default capacity, doubles
#define HM_ARENA_CHUNK_SIZE 1u<<12  // 4096 bytes par chunk

/* --- Arena Implementation and definitions ---
 *
 * Using a singly linked list for the chunks
 * Arena lifetime is bound to hashmap lifetime
 * */
typedef struct arena_chunk {
    unsigned char *base;
    size_t cap;
    size_t used;
    struct arena_chunk *next;
} hm_arena_chunk;

typedef struct {
    hm_arena_chunk *head;
    size_t default_cap;
} hm_arena;

hm_arena *hm_arena_init(size_t default_cap) {
    hm_arena *a = malloc(sizeof *a);
    a->head = NULL;
    a->default_cap = default_cap;
    return a;
}

void *hm_arena_alloc(hm_arena *a, size_t sz)
{
    hm_arena_chunk *c = a->head;

    if (c) {
        /* align allocation start (must be done before capacity check) */
        size_t off = (c->used + 7) & ~((size_t)7);

        /* ensure aligned allocation fits in chunk */
        if (off + sz <= c->cap) {
            c->used = off + sz;
            return c->base + off;
        }
    }

    /* allocate new chunk if no space */
    size_t cap = a->default_cap > sz ? a->default_cap : sz;

    hm_arena_chunk *n = malloc(sizeof *n);
    if (!n) return NULL;

    n->base = malloc(cap);
    if (!n->base) {
        free(n);
        return NULL;
    }

    n->cap  = cap;
    n->used = sz;        // first allocation, already aligned
    n->next = a->head;
    a->head = n;

    return n->base;
}

void hm_arena_free(hm_arena *a)
{
    hm_arena_chunk *s = a->head;
    while (s) {
        hm_arena_chunk *next = s->next;
        free(s->base);
        free(s);
        s = next;
    }
    a->head = NULL;
}
/* my own utility functions and string builder  */
static char *_str_arena(hm_arena *a, const char *s)
{
    size_t n=0, i=0;
    while(s[n++]!=0); // manual strlen

    char *p = hm_arena_alloc(a, n);
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

/**********/

#define FNV_OFFSET 0xcbf29ce484222325
#define FNV_PRIME  0x100000001b3

/* This returns a 64-bit (size_t) fnv-1a hash for a given key,
 * assumed to be null-terminated
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
        new_cap = HM_INITIAL_CAPACITY;      // just this once to init everything
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
    if (!hm->arena) {
        hm->arena = hm_arena_init(HM_ARENA_CHUNK_SIZE);
    }

    if (hm->count * LOAD_FACTOR_DEN >= hm->capacity * LOAD_FACTOR_NUM)
        _hm_resize(hm);

    return _hm_set_entry(hm, key, value);
}

/* Returns the value associated with key, or null */
uintptr_t hm_get(hashmap *hm, const char *key)
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
                return e->value;
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
            }
        }
        hash_index = (hash_index + 1) & (hm->capacity-1);
        e = &hm->items[hash_index];
    }
    return 0;
}

/* Frees arena, arena struct, and item array.
 * Does NOT free hashmap struct itself.
 */
void hm_destroy(hashmap *hm)
{
    hm_arena_free(hm->arena);
    free(hm->arena);
    free(hm->items);
}
