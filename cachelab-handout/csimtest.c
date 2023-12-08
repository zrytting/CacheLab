#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

// Define the structure for a cache line
typedef struct {
    int valid;
    unsigned long tag;
    int lru_counter;
} cache_line_t;

// Define a cache set as an array of cache lines
typedef struct {
    cache_line_t *lines;
} cache_set_t;

// Define the cache as an array of sets
typedef struct {
    int set_count;
    int lines_per_set;
    cache_set_t *sets;
} cache_t;

// Function to initialize the cache
cache_t init_cache(int set_count, int lines_per_set) {
    cache_t cache;
    cache.set_count = set_count;
    cache.lines_per_set = lines_per_set;
    cache.sets = (cache_set_t *)malloc(set_count * sizeof(cache_set_t));

    for (int i = 0; i < set_count; i++) {
        cache.sets[i].lines = (cache_line_t *)malloc(lines_per_set * sizeof(cache_line_t));
        for (int j = 0; j < lines_per_set; j++) {
            cache.sets[i].lines[j].valid = 0;
            cache.sets[i].lines[j].tag = 0;
            cache.sets[i].lines[j].lru_counter = 0;
        }
    }

    return cache;
}

// Function to free the memory allocated for the cache
void free_cache(cache_t cache) {
    for (int i = 0; i < cache.set_count; i++) {
        free(cache.sets[i].lines);
    }
    free(cache.sets);
}

// Function to simulate a single cache access
int simulate_access(cache_t *cache, unsigned long address, int s, int b, int *evictions) {
    unsigned long set_index = (address >> b) & ((1 << s) - 1);
    unsigned long tag = address >> (s + b);
    cache_set_t *set = &cache->sets[set_index];
    int hit = 0;
    int least_recently_used = 0;
    int lru_index = -1;

    // Check for hit
    for (int i = 0; i < cache->lines_per_set; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            hit = 1;
            set->lines[i].lru_counter = 0;
            break;
        }
    }

    // If miss, find a line to evict
    if (!hit) {
        for (int i = 0; i < cache->lines_per_set; i++) {
            if (set->lines[i].valid) {
                if (lru_index == -1 || set->lines[i].lru_counter > least_recently_used) {
                    least_recently_used = set->lines[i].lru_counter;
                    lru_index = i;
                }
            } else {
                lru_index = i;
                break;
            }
        }

        // Evict if necessary
        if (set->lines[lru_index].valid) {
            (*evictions)++;
        }

        // Update the line
        set->lines[lru_index].valid = 1;
        set->lines[lru_index].tag = tag;
        set->lines[lru_index].lru_counter = 0;
    }

    // Update LRU counters
    for (int i = 0; i < cache->lines_per_set; i++) {
        if (i != lru_index) {
            set->lines[i].lru_counter++;
        }
    }

    return hit;
}

// Function to process each line of the trace file
void process_trace_line(cache_t *cache, char operation, unsigned long address, int s, int b, int *hits, int *misses, int *evictions, int verbose) {
    int hit1 = simulate_access(cache, address, s, b, evictions);
    int hit2 = simulate_access(cache, address, s, b, evictions);
    if (hit1) {
        (*hits)++;
    } else {
        (*misses)++;
    }

    // For 'M', simulate two accesses
    if (operation == 'M') {
        int hit2 = simulate_access(cache, address, s, b, evictions);
        if (hit2) {
            (*hits)++;
        } else {
            (*misses)++;
        }
    }

    // Verbose output
    if (verbose) {
        printf("%c %lx,%d ", operation, address, 1); // Assuming size is always 1
        printf(hit1 ? "hit " : "miss ");
        if (operation == 'M') {
            printf(hit2 ? "hit\n" : "miss\n");
        } else {
            printf("\n");
        }
    }
}

int main(int argc, char *argv[]) {
    int s, E, b;
    char *tracefile;
    int opt;
    int verbose = 0;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
                return 0;
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                tracefile = optarg;
                break;
            default:
                fprintf(stderr, "Usage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
                exit(EXIT_FAILURE);
        }
    }

    if (s <= 0 || E <= 0 || b <= 0 || tracefile == NULL) {
        fprintf(stderr, "Invalid arguments.\n");
        exit(EXIT_FAILURE);
    }

    int set_count = 1 << s;
    cache_t cache = init_cache(set_count, E);
    int hits = 0, misses = 0, evictions = 0;

    FILE *fp = fopen(tracefile, "r");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file %s.\n", tracefile);
        exit(EXIT_FAILURE);
    }

    char operation;
    unsigned long address;

    // Read memory traces and simulate cache behavior
    while (fscanf(fp, " %c %lx,%*d", &operation, &address) == 2) {
        if (operation != 'I') {  // Ignore instruction loads
            process_trace_line(&cache, operation, address, s, b, &hits, &misses, &evictions, verbose);
        }
    }

    fclose(fp);
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);

    free_cache(cache);
    return 0;
}

