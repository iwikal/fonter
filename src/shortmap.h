#include <stdint.h>

struct shortmap_bucket
{
    uint16_t key;
    void *value;
};

typedef struct
{
    struct shortmap_bucket *buckets;
    uint16_t cap;
    uint16_t len;
} shortmap_t;

shortmap_t shortmap_create(uint16_t);
void *shortmap_insert(shortmap_t *, uint16_t, void *);
void *shortmap_get(shortmap_t *, uint16_t);
