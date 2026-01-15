#include <stdio.h>
#include "hash.h"

/* -- Full usage of a hashmap in c --- */

int main(void) {
    hashmap hm = {0};

    hm_put(&hm, "a", 5);
    hm_put(&hm, "b", 10);

    printf("put 'a':5 and 'b':10\n");

    int c_key = hm_contains_key(&hm, "a");
    int val = (int)hm_get(&hm, "a");

    printf("contains a? %s\n", c_key ? "yes" : "no");
    printf("value a = %d\n", val);

    int c_val1 = hm_contains_value(&hm, 10);
    int c_val2 = hm_contains_value(&hm, 99);
    printf("contains value 10? %s\n", c_val1 ? "yes" : "no");
    printf("contains value 99? %s\n", c_val2 ? "yes" : "no");

    hm_put(&hm, "a", -20);
    val = (int)hm_get(&hm, "a");
    printf("new value a = %d\n", val);

    hm_remove(&hm, "a");
    c_key = hm_contains_key(&hm, "a");
    printf("contains a after remove? %s\n", c_key? "yes" : "no");

    int remove = hm_remove(&hm, "a");
    printf("2nd remove a returns %d\n",remove);

    /* -- Testing for other types -- */
    const char *string_test = "Hi this is a test";

    hm_put(&hm, "string", (uintptr_t)string_test);
    uintptr_t string_addr = hm_get(&hm, "string");

    printf("Did this work?: %s\n", (const char*)string_addr);

    hm_destroy(&hm);
    return 0;
}
