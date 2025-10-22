#define _GNU_SOURCE
#include "hash_table.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* for strcasecmp */
#include <stdio.h>
#include <stdint.h>

/* Simple singly-linked node for chaining */
typedef struct node {
    int key;
    struct node *next;
} node_t;

/* For open addressing, we track states */
typedef enum { SLOT_EMPTY = 0, SLOT_OCCUPIED, SLOT_DELETED } slot_state_t;

struct hash_table {
    size_t size;
    collision_strategy_t strat;
    ht_stats_t stats;

    /* For chaining */
    node_t **buckets; /* array of node* of length size */

    /* For open addressing (linear/quadratic/double) */
    int *slots;               /* array length size, store key */
    slot_state_t *slot_state; /* parallel array */
};

/* Simple primary hash: mix and modulo */
static size_t primary_hash(int key, size_t size) {
    uint32_t x = (uint32_t)key;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return (size_t)(x % (uint32_t)size);
}

/* Secondary hash for double hashing: must be non-zero and relatively prime to size ideally */
static size_t secondary_hash(int key, size_t size) {
    uint32_t x = (uint32_t)key;
    /* Use a small odd function: 1 + (key mod (size-1)) */
    if (size <= 2) return 1;
    size_t r = 1 + (x % (size - 1));
    return r;
}

hash_table_t *ht_create(size_t table_size, collision_strategy_t strat) {
    hash_table_t *ht = calloc(1, sizeof(*ht));
    if (!ht) return NULL;
    ht->size = (table_size < 3) ? 3 : table_size;
    ht->strat = strat;
    ht->stats.table_size = ht->size;
    ht->stats.inserted = 0;
    ht->stats.collisions = 0;
    ht->stats.probes = 0;

    if (strat == STRAT_CHAINING) {
        ht->buckets = calloc(ht->size, sizeof(node_t *));
        if (!ht->buckets) { free(ht); return NULL; }
    } else {
        ht->slots = malloc(ht->size * sizeof(int));
        ht->slot_state = malloc(ht->size * sizeof(slot_state_t));
        if (!ht->slots || !ht->slot_state) {
            free(ht->slots); free(ht->slot_state);
            free(ht); return NULL;
        }
        for (size_t i = 0; i < ht->size; ++i) ht->slot_state[i] = SLOT_EMPTY;
    }
    return ht;
}

void ht_free(hash_table_t *ht) {
    if (!ht) return;
    if (ht->buckets) {
        for (size_t i = 0; i < ht->size; ++i) {
            node_t *n = ht->buckets[i];
            while (n) {
                node_t *t = n->next;
                free(n);
                n = t;
            }
        }
        free(ht->buckets);
    }
    if (ht->slots) free(ht->slots);
    if (ht->slot_state) free(ht->slot_state);
    free(ht);
}

int ht_insert(hash_table_t *ht, int key) {
    if (!ht) return 0;
    if (ht->strat == STRAT_CHAINING) {
        size_t idx = primary_hash(key, ht->size);
        node_t *n = malloc(sizeof(node_t));
        if (!n) return 0;
        n->key = key;
        n->next = ht->buckets[idx];
        if (ht->buckets[idx]) ht->stats.collisions++;
        ht->buckets[idx] = n;
        ht->stats.inserted++;
        return 1;
    } else {
        size_t h = primary_hash(key, ht->size);
        size_t i = 0;
        size_t first_deleted = (size_t)-1;
        size_t probes_done = 0;
        while (i < ht->size) {
            size_t idx;
            if (ht->strat == STRAT_LINEAR_PROBING) {
                idx = (h + i) % ht->size;
            } else if (ht->strat == STRAT_QUADRATIC_PROBING) {
                idx = (h + i + (i * i)) % ht->size; /* simple quadratic */
            } else { /* DOUBLE_HASHING */
                size_t h2 = secondary_hash(key, ht->size);
                idx = (h + i * h2) % ht->size;
            }
            probes_done++;
            ht->stats.probes++;
            if (ht->slot_state[idx] == SLOT_EMPTY) {
                if (first_deleted != (size_t)-1) idx = first_deleted;
                ht->slots[idx] = key;
                ht->slot_state[idx] = SLOT_OCCUPIED;
                if (probes_done > 1) ht->stats.collisions++;
                ht->stats.inserted++;
                return 1;
            } else if (ht->slot_state[idx] == SLOT_DELETED) {
                if (first_deleted == (size_t)-1) first_deleted = idx;
            } else if (ht->slot_state[idx] == SLOT_OCCUPIED) {
                if (ht->slots[idx] == key) {
                    /* duplicate, ignore insert (could be considered update) */
                    return 1;
                }
                /* collision, continue probing */
            }
            i++;
        }
        /* Table full or no slot found */
        return 0;
    }
}

int ht_search(hash_table_t *ht, int key) {
    if (!ht) return 0;
    if (ht->strat == STRAT_CHAINING) {
        size_t idx = primary_hash(key, ht->size);
        node_t *n = ht->buckets[idx];
        while (n) { if (n->key == key) return 1; n = n->next; }
        return 0;
    } else {
        size_t h = primary_hash(key, ht->size);
        size_t i = 0;
        while (i < ht->size) {
            size_t idx;
            if (ht->strat == STRAT_LINEAR_PROBING) {
                idx = (h + i) % ht->size;
            } else if (ht->strat == STRAT_QUADRATIC_PROBING) {
                idx = (h + i + (i * i)) % ht->size;
            } else {
                size_t h2 = secondary_hash(key, ht->size);
                idx = (h + i * h2) % ht->size;
            }
            ht->stats.probes++;
            if (ht->slot_state[idx] == SLOT_EMPTY) return 0;
            if (ht->slot_state[idx] == SLOT_OCCUPIED && ht->slots[idx] == key) return 1;
            i++;
        }
        return 0;
    }
}

int ht_remove(hash_table_t *ht, int key) {
    if (!ht) return 0;
    if (ht->strat == STRAT_CHAINING) {
        size_t idx = primary_hash(key, ht->size);
        node_t *prev = NULL;
        node_t *n = ht->buckets[idx];
        while (n) {
            if (n->key == key) {
                if (prev) prev->next = n->next;
                else ht->buckets[idx] = n->next;
                free(n);
                ht->stats.inserted--;
                return 1;
            }
            prev = n;
            n = n->next;
        }
        return 0;
    } else {
        size_t h = primary_hash(key, ht->size);
        size_t i = 0;
        while (i < ht->size) {
            size_t idx;
            if (ht->strat == STRAT_LINEAR_PROBING) {
                idx = (h + i) % ht->size;
            } else if (ht->strat == STRAT_QUADRATIC_PROBING) {
                idx = (h + i + (i * i)) % ht->size;
            } else {
                size_t h2 = secondary_hash(key, ht->size);
                idx = (h + i * h2) % ht->size;
            }
            if (ht->slot_state[idx] == SLOT_EMPTY) return 0;
            if (ht->slot_state[idx] == SLOT_OCCUPIED && ht->slots[idx] == key) {
                ht->slot_state[idx] = SLOT_DELETED;
                ht->stats.inserted--;
                return 1;
            }
            i++;
        }
        return 0;
    }
}

const ht_stats_t *ht_get_stats(hash_table_t *ht) {
    if (!ht) return NULL;
    return &ht->stats;
}

static const char *strategy_name(collision_strategy_t s) {
    switch (s) {
        case STRAT_CHAINING: return "CHAINING";
        case STRAT_LINEAR_PROBING: return "LINEAR_PROBING";
        case STRAT_QUADRATIC_PROBING: return "QUADRATIC_PROBING";
        case STRAT_DOUBLE_HASHING: return "DOUBLE_HASHING";
        default: return "UNKNOWN";
    }
}

int ht_write_snapshot_json(hash_table_t *ht, FILE *out) {
    if (!ht || !out) return 0;
    fprintf(out, "{\n");
    fprintf(out, "  \"strategy\": \"%s\",\n", strategy_name(ht->strat));
    fprintf(out, "  \"table_size\": %zu,\n", ht->size);
    fprintf(out, "  \"inserted\": %zu,\n", ht->stats.inserted);
    fprintf(out, "  \"collisions\": %zu,\n", ht->stats.collisions);
    fprintf(out, "  \"probes\": %zu,\n", ht->stats.probes);
    fprintf(out, "  \"buckets\": [\n");
    for (size_t i = 0; i < ht->size; ++i) {
        if (ht->strat == STRAT_CHAINING) {
            node_t *n = ht->buckets[i];
            if (!n) {
                fprintf(out, "    null");
            } else {
                fprintf(out, "    [");
                int first = 1;
                while (n) {
                    if (!first) fprintf(out, ", ");
                    fprintf(out, "%d", n->key);
                    first = 0;
                    n = n->next;
                }
                fprintf(out, "]");
            }
        } else {
            if (ht->slot_state[i] == SLOT_EMPTY) {
                fprintf(out, "    null");
            } else if (ht->slot_state[i] == SLOT_DELETED) {
                fprintf(out, "    {\"deleted\": true}");
            } else {
                fprintf(out, "    %d", ht->slots[i]);
            }
        }
        if (i + 1 < ht->size) fprintf(out, ",\n");
        else fprintf(out, "\n");
    }
    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
    return 1;
}

collision_strategy_t ht_strategy_from_string(const char *s) {
    if (!s) return STRAT_CHAINING;
    if (strcasecmp(s, "CHAINING") == 0) return STRAT_CHAINING;
    if (strcasecmp(s, "LINEAR") == 0 || strcasecmp(s, "LINEAR_PROBING") == 0) return STRAT_LINEAR_PROBING;
    if (strcasecmp(s, "QUADRATIC") == 0 || strcasecmp(s, "QUADRATIC_PROBING") == 0) return STRAT_QUADRATIC_PROBING;
    if (strcasecmp(s, "DOUBLE") == 0 || strcasecmp(s, "DOUBLE_HASHING") == 0) return STRAT_DOUBLE_HASHING;
    return STRAT_CHAINING;
}