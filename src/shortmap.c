#include <stdlib.h>
#include "shortmap.h"

shortmap_t shortmap_create(uint16_t initial_cap)
{
    shortmap_t map;
    map.cap = initial_cap;
    map.len = 0;
    map.buckets = malloc(sizeof(struct shortmap_bucket) * initial_cap);
    for (int i = 0; i < initial_cap; i++)
    {
        map.buckets[i].key = 0;
        map.buckets[i].value = NULL;
    }
    return map;
}

void *shortmap_get(shortmap_t *map, uint16_t key)
{
    uint16_t hash = key;
    for (int i = 0; i < map->cap; i++)
    {
        hash &= map->cap - 1;
        if (map->buckets[hash].value == NULL)
            return NULL;
        if (map->buckets[hash].key == key)
            return map->buckets[hash].value;
        hash += 1;
    }
    return NULL;
}

void *shortmap_insert(shortmap_t *map, uint16_t key, void *value)
{
    // resize and rehash when we reach 75% of capacity
    if (map->len > (map->cap + map->cap / 2) / 2)
    {
        struct shortmap_bucket *old_buckets = map->buckets;
        uint16_t old_cap = map->cap;
        map->cap *= 2;
        map->len = 0;
        map->buckets = calloc(map->cap, sizeof(struct shortmap_bucket));
        for (int i = 0; i < old_cap; i++)
            if (old_buckets[i].value)
                shortmap_insert(map, old_buckets[i].key, old_buckets[i].value);
        free(old_buckets);
    }

    map->len++;

    // round robin
    uint16_t hash = key;
    for (;;)
    {
        hash &= map->cap - 1;
        if (map->buckets[hash].value == NULL)
        {
            map->buckets[hash].key = key;
            map->buckets[hash].value = value;
            return &map->buckets[hash];
        }
        hash += 1;
    }
}
