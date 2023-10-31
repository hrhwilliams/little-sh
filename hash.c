#include <stdlib.h>
#include <string.h>

#include "quash.h"
#include "hash.h"

static void free_bucket_list(JobHashTableNode *bucket) {
    if (!bucket) {
        return;
    }

    if (bucket->next) {
        free_bucket_list(bucket->next);
    }

    free(bucket);
}

static void free_bucket(JobHashTableNode *bucket) {
    free_bucket_list(bucket->next);
}

void init_hash_table(JobHashTable *table) {
    memset(table, 0, sizeof *table);
}

void free_hash_table_buckets(JobHashTable *table) {
    for (int i = 0; i < TABLE_BUCKETS; i++) {
        free_bucket(&table->buckets[i]);
    }
}

void hash_table_insert(JobHashTable *table, pid_t key, Job *value) {
    size_t bucket = key & (TABLE_BUCKETS - 1);

    if (table->buckets[bucket].key == 0) {
        table->buckets[bucket].key = key;
        table->buckets[bucket].value = value;
    } else {
        JobHashTableNode *node = &table->buckets[bucket];

        for (;;) {
            if (node->next) {
                node = node->next;
            } else {
                break;
            }
        }

        node->next = malloc(sizeof *node);
        node->next->prev = node;
        node = node->next;

        node->key = key;
        node->value = value;
        node->next = NULL;
    }

    table->elements++;
}

Job* hash_table_get(JobHashTable *table, pid_t key) {
    JobHashTableNode *node = &table->buckets[key & (TABLE_BUCKETS - 1)];

    for (;;) {
        if (node->key == key) {
            return node->value;
        }

        if (node->next) {
            node = node->next;
        } else {
            break;
        }
    }

    return NULL;
}

int hash_table_delete(JobHashTable *table, pid_t key) {
    size_t bucket = key & (TABLE_BUCKETS - 1);
    JobHashTableNode *node = &table->buckets[bucket];

    if (table->buckets[bucket].key == key) {
        if (table->buckets[bucket].next) {
            JobHashTableNode *next_node = table->buckets[bucket].next;
            table->buckets[bucket].key = next_node->key;
            table->buckets[bucket].value = next_node->value;
            table->buckets[bucket].next = next_node->next;

            if (table->buckets[bucket].next) {
                table->buckets[bucket].next->prev = &table->buckets[bucket];
            }

            free(next_node);
        } else {
            table->buckets[bucket].key = 0;
            table->buckets[bucket].value = NULL;
        }
        return 1;
    }

    for (;;) {
        if (node->key == key) {
            node->prev->next = node->next;
            if (node->next) {
                node->next->prev = node->prev;
            }

            free(node);
            return 1;
        }

        if (node->next) {
            node = node->next;
        } else {
            break;
        }
    }

    return 0;
}
