#include <stdio.h>
#include <stdlib.h>

#include "quash.h"
#include "hash.h"

JobHashTable table;

int main() {
    init_hash_table(&table);

    for (size_t i = 1; i <= 256; i++)
        hash_table_insert(&table, i, (void*) i);
    
    for (size_t i = 1; i <= 256; i++) {
        Job* job = hash_table_get(&table, i);
        if ((size_t) job != i) {
            printf("value doesn't match\n");
        }
    }

    for (size_t i = 1; i <= 256; i++)
        hash_table_delete(&table, i);

    for (size_t i = 1; i <= 256; i++) {
        Job* job = hash_table_get(&table, i);
        if (job) {
            printf("got a deleted value somehow\n");
        }
    }

    free_hash_table_buckets(&table);
}
