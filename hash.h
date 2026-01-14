/* Copyright (c) 2026 Andreas B. Nore <github.com/abnore>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef HASHMAP_H
#define HASHMAP_H
/*
 *
 *      NOT SAFE FOR PRODUCTION! - (But go ahead if you want)
 *
 *
 * My implementation of a dynamic hashmap in C. Like the Python dict or the Java
 * HashMap it has key-value pairs and dynamic allocation.
 *
 * Here we have keys in the form of strings and values are numbers, stored as
 * uintptr_t, which is an integer type guaranteed to be the size of a pointer.
 * Pointers are, as far as i know, always the architecture size, e.g. 64bit
 * (8bytes) on most new machines. Therefore I can store raw numbers, or pointers
 * as `values`, just like Go does with "any".
 *
 *      **Will update later for hashmaps for any types**
 *
 * How to use:
 *    - No initiation needed, you only create a hashmap either on the stack or on
 *      the heap, works either way. Only need to set capacity to 0 initially.
 *      This can be done by creating the hashmap like so
 *
 *          hashmap hm = {0};
 *
 *      or with the older style
 *
 *          hashmap hm = { .capacity = 0 };
 *
 *      Either will set everything to 0, and this will enable the hm_put to set
 *      256 spots, which will grow by a factor of 2 with load factor of 70%,
 *      which means at 70% percent full the capacity will double.
 *      This ensures fewer collisions and faster lookups, while at the same time
 *      not being too memory expensive.
 *
 *    - hm_put will return either 0 or 1.
 *      hm_put will return a 1 when it overwrote the same key, 0 means success.
 *      Neither will indicate a hash collision.
 *    - hm_get will return the value in the key value pair or 0 if not found.
 *      that does mean that if you store a 0 you will get your value no matter what
 *    - hm_remove returns a 1 if successful, 0 if not found
 *    - hm_destroy will free everything and ensure no memory leaks
 *
 *    - hm_contains_key will attempt to search by key, should return 1 if found
 *      and 0 if not. If not found every spot will have been checked, making this
 *      the same as a linear search.
 *
 *    - hm_contains_value can only use a linear search and will go through every
 *      position and return 1 if found, 0 if not.
 *
 * Internally we use linear probing and the fuller the array gets, the closer
 * to linear it will become. Therefore it is never more than 70% full.
 *
 * Benchmarking on a Macbook M1 Pro gave this as the best results with the
 * accompanying test:
 *
 *     Insert:  200k in ~20ms  (~10 Mops/sec)
 *     Lookup:  200k in ~19ms (~10.5 Mops/sec)
 *     Remove:  200k in ~18ms (~11 Mops/sec)
 *
 * Which is pretty OK for a hand-rolled, simple-arena, linear-probe hash map
 * in plain C. There are better hashes out there and better ways to store strings.
 *
 * */

typedef struct {
    unsigned char *base;
    size_t cap;
    size_t used;
} hm_arena;

typedef struct{
    char *key;
    uintptr_t value;
    size_t hash;
}hm_entry;

typedef struct{
    hm_arena *arena;
    hm_entry *items;
    size_t capacity;
    size_t count;
}hashmap;

// Checks if the map contains a map to key, 1=yes, 0=no
int hm_contains_key(hashmap *hm, const char *key);

// Checks if the map contains one or more keys mapped to value.
int hm_contains_value(hashmap *hm, uintptr_t value);

// Inserts a key-value pair into the map. 1 if overwrite, 0 else
int hm_put(hashmap *hm, const char *key, uintptr_t value);

// Returns the value associated with key, or 0 if not found (or if value 0)
int hm_get(hashmap *hm, const char *key);

// Removes the mapping for key (1 if removes, 0 not found)
int hm_remove(hashmap *hm, const char *key);

// Destroy hashmap, freeing all allocated memory and arena
void hm_destroy(hashmap *hm);

/* --- arena functions --- */
// Created a new arena with the size of cap
hm_arena arena_new(size_t cap);

// Allocated memory, instead of malloc, into the arena
void *arena_alloc(hm_arena *a, size_t sz);

// Deallocated the entire arena
void arena_free(hm_arena *a);

#endif // HASHMAP_H
