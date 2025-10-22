/*
Prototype driver:
 - Generates keys with different distributions
 - Inserts into selected strategy
 - Emits JSON snapshots into ./ui/snapshots/ and updates manifest.json

Usage examples:
  make
  ./bin/ht --strategy=LINEAR --size=101 --inserts=200 --dist=uniform --interval=5

Then serve ui/ as static files (e.g. python3 -m http.server) and open ui/index.html
*/
#include "hash_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#endif

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--strategy=CHAINING|LINEAR|QUADRATIC|DOUBLE] [--size=N] [--inserts=N] [--dist=sequential|uniform|clustered] [--interval=M]\n",
        prog);
}

/* Ensure directory exists (simple, POSIX). */
static int ensure_dir(const char *path) {
#ifdef _WIN32
    /* Minimal windows support */
    if (_mkdir(path) == 0) return 1;
    return 0;
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        if ((st.st_mode & S_IFDIR) != 0) return 1;
        return 0;
    }
    if (mkdir(path, 0755) == 0) return 1;
    return 0;
#endif
}

/* Overwrite manifest.json with filenames */
static int write_manifest(const char *snapshots_dir, char **files, size_t count) {
    char manifest_path[1024];
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", snapshots_dir);
    FILE *f = fopen(manifest_path, "w");
    if (!f) return 0;
    fprintf(f, "{\n  \"snapshots\": [\n");
    for (size_t i = 0; i < count; ++i) {
        fprintf(f, "    \"%s\"", files[i]);
        if (i + 1 < count) fprintf(f, ",\n");
        else fprintf(f, "\n");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    return 1;
}

/* Key generators */
static int next_key_sequential(int idx) { return idx + 1; }
static int next_key_uniform(int idx) { return rand() % (idx + 1000) + 1; }
static int next_key_clustered(int idx) {
    int center = (rand() % 5) * 50 + 1;
    return center + (rand() % 20);
}

int main(int argc, char **argv) {
    size_t size = 101;
    size_t inserts = 100;
    int interval = 1;
    collision_strategy_t strat = STRAT_CHAINING;
    char dist[32] = "sequential";

    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--strategy=", 11) == 0) strat = ht_strategy_from_string(argv[i] + 11);
        else if (strncmp(argv[i], "--size=", 7) == 0) size = (size_t)atoi(argv[i] + 7);
        else if (strncmp(argv[i], "--inserts=", 10) == 0) inserts = (size_t)atoi(argv[i] + 10);
        else if (strncmp(argv[i], "--dist=", 7) == 0) strncpy(dist, argv[i] + 7, sizeof(dist)-1);
        else if (strncmp(argv[i], "--interval=", 11) == 0) interval = atoi(argv[i] + 11);
        else if (strcmp(argv[i], "--help") == 0) { usage(argv[0]); return 0; }
    }

    if (interval <= 0) interval = 1;

    srand((unsigned)time(NULL));

    const char *snapshots_dir = "ui/snapshots";
    if (!ensure_dir("ui")) { fprintf(stderr, "Failed to ensure ui/ directory\n"); return 1; }
    if (!ensure_dir(snapshots_dir)) { fprintf(stderr, "Failed to ensure ui/snapshots/ directory\n"); return 1; }

    hash_table_t *ht = ht_create(size, strat);
    if (!ht) { fprintf(stderr, "Failed to create hash table\n"); return 1; }

    size_t max_snapshots = (inserts / (size_t)interval) + 10;
    char **manifest_files = malloc(max_snapshots * sizeof(char*));
    if (!manifest_files) { fprintf(stderr, "Allocation failure\n"); ht_free(ht); return 1; }
    size_t manifest_count = 0;
    int last_snap_idx = -1;

    for (size_t i = 0; i < inserts; ++i) {
        int key;
        if (strcmp(dist, "sequential") == 0) key = next_key_sequential((int)i);
        else if (strcmp(dist, "uniform") == 0) key = next_key_uniform((int)i);
        else key = next_key_clustered((int)i);

        ht_insert(ht, key);

        if (interval > 0 && ((int)i % interval == 0)) {
            char fname[1024];
            snprintf(fname, sizeof(fname), "snap_%05zu.json", manifest_count);
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", snapshots_dir, fname);
            FILE *f = fopen(path, "w");
            if (!f) {
                fprintf(stderr, "Failed to write snapshot %s: %s\n", path, strerror(errno));
            } else {
                ht_write_snapshot_json(ht, f);
                fclose(f);
                manifest_files[manifest_count] = strdup(fname);
                manifest_count++;
                last_snap_idx = (int)i;
                write_manifest(snapshots_dir, manifest_files, manifest_count);
                printf("Wrote snapshot %s\n", path);
            }
        }
    }

    /* final snapshot if not already captured at last insert */
    if (last_snap_idx != (int)(inserts - 1)) {
        char fname[1024];
        snprintf(fname, sizeof(fname), "snap_%05zu.json", manifest_count);
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", snapshots_dir, fname);
        FILE *f = fopen(path, "w");
        if (f) {
            ht_write_snapshot_json(ht, f);
            fclose(f);
            manifest_files[manifest_count] = strdup(fname);
            manifest_count++;
            write_manifest(snapshots_dir, manifest_files, manifest_count);
            printf("Wrote final snapshot %s\n", path);
        }
    }

    /* cleanup */
    for (size_t i = 0; i < manifest_count; ++i) free(manifest_files[i]);
    free(manifest_files);
    ht_free(ht);
    return 0;
}