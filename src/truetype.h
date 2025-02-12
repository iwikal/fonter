#include <stdint.h>

#ifndef TRUETYPE_H
#define TRUETYPE_H

typedef uint32_t Fixed;
typedef uint64_t Date;
typedef int16_t FWord;
typedef uint16_t UFWord;

typedef enum { OK, ERR } RESULT;

struct cmap_4
{
    uint16_t seg_count;
    uint16_t tail[];
};

struct hhea
{
  FWord ascender;
  FWord descender;
  FWord line_gap;
  UFWord advance_width_max;
  FWord min_left_side_bearing;
  FWord min_right_side_bearing;
  FWord x_max_extent;
  int16_t caret_slope_rise;
  int16_t caret_slope_run;
  int16_t caret_offset;
  int16_t metric_data_format;
  uint16_t num_h_metrics;
};

struct ttf_reader
{
    void *data;
    void *cursor;
    uint16_t num_glyphs;
    uint32_t *locations;
    void *glyphs;
    struct cmap_4 *cmap;
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
RESULT ttf_parse_head(struct ttf_reader *, uint16_t *);
RESULT ttf_parse_maxp(struct ttf_reader *);
RESULT ttf_parse_loca(struct ttf_reader *, uint16_t);
RESULT ttf_parse_glyf(struct ttf_reader *, uint16_t, struct ttf_glyph *);
RESULT ttf_parse_cmap(struct ttf_reader *);
RESULT ttf_parse_hhea(struct ttf_reader *);

uint16_t ttf_lookup_index(struct cmap_4 *, uint16_t c);
int ttf_num_points(struct ttf_glyph *);

#endif // TRUETYPE_H
