#include <stdint.h>

#ifndef TRUETYPE_H
#define TRUETYPE_H

typedef uint32_t Fixed;
typedef uint64_t Date;
typedef int16_t FWord;
typedef uint16_t UFWord;

typedef enum { OK = 0, ERR = -1 } RESULT;

struct cmap_4
{
    uint16_t seg_count;
    uint16_t tail[];
};

struct ttf_reader
{
    void *data;
    void *cursor;
    void *glyphs;
    void *hmetrics;
    struct cmap_4 *cmap;
    uint32_t *locations;
    uint16_t num_glyphs;
    int16_t num_hmetrics;
    uint16_t units_per_em;
};

typedef struct
{
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;
} bbox_t;

typedef struct
{
    int c[2];
    int on_curve;
} contour_point_t;

struct ttf_glyph
{
    int16_t num_contours;
    bbox_t bbox;
    contour_point_t *points;
    uint16_t *contour_endpoints;
};

RESULT ttf_parse(struct ttf_reader *);
RESULT ttf_parse_head(struct ttf_reader *, int16_t *);
RESULT ttf_parse_maxp(struct ttf_reader *);
RESULT ttf_parse_loca(struct ttf_reader *, uint16_t);
RESULT ttf_parse_glyf(struct ttf_reader *, uint16_t, struct ttf_glyph *);
RESULT ttf_parse_cmap(struct ttf_reader *);
RESULT ttf_parse_hhea(struct ttf_reader *);
RESULT ttf_parse_hmtx(struct ttf_reader *);

uint16_t ttf_lookup_index(struct cmap_4 *, uint16_t c);
int ttf_num_points(struct ttf_glyph *);

#endif // TRUETYPE_H
