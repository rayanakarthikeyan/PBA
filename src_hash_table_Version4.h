/*
Simple dynamic hash table prototype header.
Supports four collision strategies:
 - CHAINING
 - LINEAR_PROBING
 - QUADRATIC_PROBING
 - DOUBLE_HASHING

This is a compact educational prototype intended to produce JSON snapshots that a web UI can visualize.
*/
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
    STRAT_CHAINING = 0,
    STRAT_LINEAR_PROBING,
    STRAT_QUADRATIC_PROBING,
    STRAT_DOUBLE_HASHING
} collision_strategy_t;

typedef struct ht_stats {
    size_t table_size;
    size_t inserted;
    size_t collisions;
    size_t probes;     // total probes performed during insert/search ops
} ht_stats_t;

/* Opaque hash table handle */
typedef struct hash_table hash_table_t;

/* Create a hash table with given size and strategy */
hash_table_t *ht_create(size_t table_size, collision_strategy_t strat);

/* free resources */
void ht_free(hash_table_t *ht);

/* Insert a key. Returns 1 on success, 0 if table full or memory failure (for chaining) */
int ht_insert(hash_table_t *ht, int key);

/* Search for key: returns 1 if found, 0 otherwise */
int ht_search(hash_table_t *ht, int key);

/* Remove key: returns 1 if removed, 0 if not found */
int ht_remove(hash_table_t *ht, int key);

/* Return current stats snapshot (caller must not modify) */
const ht_stats_t *ht_get_stats(hash_table_t *ht);

/*
Write a JSON snapshot of the table to the provided FILE*.
See implementation for JSON structure.
*/
int ht_write_snapshot_json(hash_table_t *ht, FILE *out);

/* Helper: convert strategy enum from string */
collision_strategy_t ht_strategy_from_string(const char *s);

#endif