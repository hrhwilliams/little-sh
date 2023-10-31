#ifndef __QUASH_HASH_H__
#define __QUASH_HASH_H__

#include "quash.h"

void init_hash_table(JobHashTable *table);
void free_hash_table_buckets(JobHashTable *table);
void hash_table_insert(JobHashTable *table, pid_t key, Job *value);
Job* hash_table_get(JobHashTable *table, pid_t key);
int hash_table_delete(JobHashTable *table, pid_t key);

#endif /* __QUASH_HASH_H__ */
