#include <stdlib.h>
#include <error.h>
#include <stdio.h>
#include <stdbool.h>
#include "truetype.h"

typedef uint32_t Fixed;
typedef uint64_t Date;
typedef int16_t FWord;
typedef uint16_t UFWord;

struct cmap_arrs
{
    uint16_t *end_code;
    uint16_t *start_code;
    uint16_t *id_delta;
    uint16_t *id_range_offset;
};

enum
{
    PLATFORM_UNICODE,
    PLATFORM_MACINTOSH,
    PLATFORM_ISO,
    PLATFORM_WINDOWS,
    PLATFORM_CUSTOM,
};

enum
{
    TAG_cmap               = 0x636d6170,
    TAG_cvt                = 0x63767420,
    TAG_fpgm               = 0x6670676d,
    TAG_gasp               = 0x67617370,
    TAG_gdef               = 0x47444546,
    TAG_glyf               = 0x676c7966,
    TAG_gpos               = 0x47504f53,
    TAG_gsub               = 0x47535542,
    TAG_head               = 0x68656164,
    TAG_hhea               = 0x68686561,
    TAG_hmtx               = 0x686d7478,
    TAG_loca               = 0x6c6f6361,
    TAG_maxp               = 0x6d617870,
    TAG_name               = 0x6e616d65,
    TAG_os_2               = 0x4f532f32,
    TAG_otto               = 0x4f54544f,
    TAG_post               = 0x706f7374,
    TAG_prep               = 0x70726570,
    TAG_truetype           = 0x00010000,
    TAG_truetypecollection = 0x74746366,
};

uint8_t read_8(struct ttf_reader *reader)
{
    return *(uint8_t *) reader->cursor++;
}

uint16_t read_16(struct ttf_reader *reader)
{
    uint16_t ms, ls;
    ms = read_8(reader);
    ls = read_8(reader);
    return ms << 8 | ls;
}

float read_f2dot14(struct ttf_reader *reader)
{
    return ((float) (int16_t) read_16(reader)) / 16384.0;
}

uint32_t read_32(struct ttf_reader *reader)
{
    uint32_t ms, ls;
    ms = read_16(reader);
    ls = read_16(reader);
    return ms << 16 | ls;
}

uint64_t read_date(struct ttf_reader *reader)
{
    uint64_t ms, ls;
    ms = read_32(reader);
    ls = read_32(reader);
    return ms << 32 | ls;
}

bbox_t read_bbox(struct ttf_reader *reader)
{
    bbox_t result;
    result.x_min = read_16(reader);
    result.y_min = read_16(reader);
    result.x_max = read_16(reader);
    result.y_max = read_16(reader);
    return result;
}

RESULT ttf_parse_head(struct ttf_reader *reader, int16_t *loc_format)
{
    if (reader->cursor == NULL)
    {
        fprintf(stderr, "no head!\n");
        return ERR;
    }

    if (read_32(reader) != 0x10000) return ERR; // check version
    read_32(reader); // skip font revision
    read_32(reader); // skip checksum adjustment
    if (read_32(reader) != 0x5f0f3cf5) return ERR; // check magic
    read_16(reader); // skip flags
    reader->units_per_em = read_16(reader); // skip units per em

    read_date(reader); // skip created
    read_date(reader); // skip modified

    read_16(reader); // skip bounding box
    read_16(reader); // skip bounding box
    read_16(reader); // skip bounding box
    read_16(reader); // skip bounding box

    read_16(reader); // skip mac style
    read_16(reader); // skip lowest recommended PPEM
    read_16(reader); // skip font direction hint
    *loc_format = read_16(reader);

    return OK;
}

RESULT ttf_parse_maxp(struct ttf_reader *reader)
{
    if (reader->cursor == NULL)
    {
        fprintf(stderr, "no maxp!\n");
        return ERR;
    }

    uint32_t version = read_32(reader);
    if (version != 0x00010000)
    {
        fprintf(stderr, "wrong maxp version 0x%x\n", version);
        return ERR;
    }
    reader->num_glyphs = read_16(reader);
    return OK;
}

RESULT ttf_parse_loca(struct ttf_reader *reader, uint16_t loc_format)
{
    if (reader->cursor == NULL)
    {
        fprintf(stderr, "no loca!\n");
        return ERR;
    }

    size_t size = sizeof(*reader->locations) * (reader->num_glyphs + 1);
    reader->locations = malloc(size);

    for (int i = 0; i < reader->num_glyphs; i++)
    {
        if (loc_format == 0)
            reader->locations[i] = read_16(reader) * 2;
        else
            reader->locations[i] = read_32(reader);
    }

    return OK;
}

RESULT ttf_parse_cmap(struct ttf_reader *reader)
{
    if (reader->cursor == NULL)
    {
        fprintf(stderr, "no cmap!\n");
        return ERR;
    }
    void *cmap = reader->cursor;

    uint16_t version = read_16(reader);
    if (version != 0)
    {
        fprintf(stderr, "wrong cmap version 0x%x\n", version);
        return ERR;
    }

    uint16_t num_tables = read_16(reader);
    void *subtable = NULL;

    for (int i = 0; i < num_tables; i++)
    {
        uint32_t ids = read_32(reader);
        uint32_t subtable_offset = read_32(reader);
        if (ids == 0x00000003) // unicode, BMP
        {
            subtable = cmap + subtable_offset;
            break;
        }
    }

    if (subtable == NULL)
    {
        fprintf(stderr, "no suitable cmap found\n");
        return ERR;
    }

    reader->cursor = subtable;
    uint16_t format = read_16(reader);
    if (format != 4)
    {
        fprintf(stderr, "unknown cmap subtable format 0x%x\n", format);
        return ERR;
    }

    size_t length = read_16(reader);
    read_16(reader); // skip language
    uint16_t seg_count = read_16(reader) / 2;
    read_16(reader); // skip search range
    read_16(reader); // skip entry selector
    read_16(reader); // skip range shift

    size_t tail_len = subtable + length - reader->cursor;
    struct cmap_4 *sub = malloc(sizeof(struct cmap_4) + tail_len);

    sub->seg_count = seg_count;
    for (int i = 0; i < tail_len / sizeof(uint16_t); i++)
        sub->tail[i] = read_16(reader);

    reader->cmap = sub;
    return OK;
}

struct cmap_arrs ttf_cmap_arrays(struct cmap_4 *cmap)
{
    struct cmap_arrs out;
    int i = 0;

    out.end_code = &cmap->tail[i];
    i += cmap->seg_count + 1;
    out.start_code = &cmap->tail[i];
    i += cmap->seg_count;
    out.id_delta = &cmap->tail[i];
    i += cmap->seg_count;
    out.id_range_offset = &cmap->tail[i];

    return out;
}

RESULT ttf_parse(struct ttf_reader *reader)
{
    uint32_t magic = read_32(reader);
    switch (magic)
    {
        case TAG_truetype:
            break;
        case TAG_truetypecollection:
            fprintf(stderr, "expected a TTF, not a TTC\n");
            return ERR;
        default:
            fprintf(stderr, "unknown magic 0x%x\n", magic);
            return ERR;
    }

    uint16_t num_tables = read_16(reader);

    read_16(reader); // skip search range
    read_16(reader); // skip entry selector
    read_16(reader); // skip range shift

    void *head = NULL, *maxp = NULL, *hhea = NULL, *hmtx = NULL,
         *cmap = NULL, *loca = NULL, *glyf = NULL;

    for (int i = 0; i < num_tables; i++)
    {
        uint32_t tag = read_32(reader);
        read_32(reader); // ignore checksum 
        uint32_t offset = read_32(reader);
        read_32(reader); // ignore length 

        void *table = reader->data + offset;
        switch (tag)
        {
            case TAG_head: head = table; break;
            case TAG_maxp: maxp = table; break;
            case TAG_hhea: hhea = table; break;
            case TAG_hmtx: hmtx = table; break;
            case TAG_cmap: cmap = table; break;
            case TAG_loca: loca = table; break;
            case TAG_glyf: glyf = table; break;
            default: break; // ignore other tables
        }
    }

    int16_t loc_format;
    reader->cursor = head;
    if (ttf_parse_head(reader, &loc_format)) return ERR;

    reader->cursor = maxp;
    if (ttf_parse_maxp(reader)) return ERR;

    reader->cursor = loca;
    if (ttf_parse_loca(reader, loc_format)) return ERR;

    reader->cursor = cmap;
    if (ttf_parse_cmap(reader)) return ERR;

    reader->cursor = hhea;
    if (ttf_parse_hhea(reader)) return ERR;

    reader->cursor = hmtx;
    if (ttf_parse_hmtx(reader)) return ERR;

    reader->glyphs = glyf;

    return OK;
}

RESULT ttf_parse_hhea(struct ttf_reader *reader)
{
    if (read_32(reader) != 0x10000)
    {
        error(0, 0, "wrong magic");
        return ERR;
    }

    /*
    FWord ascender           = read_16(reader);
    FWord descender          = read_16(reader);
    FWord line_gap           = read_16(reader);
    UFWord advance_max       = read_16(reader);
    FWord min_lsb            = read_16(reader);
    FWord min_rsb            = read_16(reader);
    FWord x_max_extent       = read_16(reader);
    int16_t caret_slope_rise = read_16(reader);
    int16_t caret_slope_run  = read_16(reader);
    int16_t caret_offset     = read_16(reader);
    read_16(reader); // reserved
    read_16(reader); // reserved
    read_16(reader); // reserved
    read_16(reader); // reserved
    read_16(reader); // 0 for current format
    */

    reader->cursor += sizeof(int16_t) * 15;
    reader->num_hmetrics = read_16(reader);

    return OK;
}

RESULT ttf_parse_hmtx(struct ttf_reader *reader)
{
    reader->hmetrics = malloc(sizeof(*reader->hmetrics) * reader->num_hmetrics);

    int i = 0;
    for (; i < reader->num_hmetrics; i++)
    {
        reader->hmetrics[i].advance_width = read_16(reader);
        reader->hmetrics[i].left_side_bearing = read_16(reader);
    }

    for (; i < reader->num_glyphs; i++)
    {
        reader->hmetrics[i].advance_width = reader->hmetrics[i - 1].advance_width;
        reader->hmetrics[i].left_side_bearing = read_16(reader);
    }

    return OK;
}

uint16_t ttf_lookup_index(struct cmap_4 *cmap, uint16_t c)
{
    struct cmap_arrs arrs = ttf_cmap_arrays(cmap);

    // Binary search
    int a = 0, b = cmap->seg_count, mid;
    while (a < b)
    {
        mid = (a + b) / 2;
        if (arrs.end_code[mid] < c)
            a = mid + 1;
        else if (arrs.start_code[mid] <= c)
            break;
        else
            b = mid;
    }

    // Return default if no range contains our character
    if (c < arrs.start_code[mid] || arrs.end_code[mid] < c) return 0;

    // What follows is beyond my understanding
    if (arrs.id_range_offset[mid] == 0)
        return arrs.id_delta[mid] + c;
    else
        return *(arrs.id_range_offset[mid]/2
                + (c - arrs.start_code[mid])
                + &arrs.id_range_offset[mid]);
}

enum
{
    ON_CURVE_POINT = 0x01,
    X_SHORT_VECTOR = 0x02,
    Y_SHORT_VECTOR = 0x04,
    REPEAT_FLAG    = 0x08,
    X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR = 0x10,
    Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR = 0x20,
    OVERLAP_SIMPLE = 0x40,
};

enum { X, Y };

void ttf_parse_coordinates(struct ttf_reader *reader,
                           uint8_t *flags,
                           size_t flags_len,
                           int axis,
                           contour_point_t *out)
{
    uint8_t short_vector = axis == X
        ? X_SHORT_VECTOR
        : Y_SHORT_VECTOR;
    uint8_t same_or_pos = axis == X
        ? X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR
        : Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR;

    for (int i = 0; i < flags_len; i++)
    {
        uint8_t flag = flags[i];

        out[i].c[axis] = i == 0 ? 0 : out[i - 1].c[axis];
        out[i].on_curve = (flag & ON_CURVE_POINT) != 0;

        if (flag & short_vector)
        {
            uint8_t delta = read_8(reader);
            int sign = flag & same_or_pos ? 1 : -1;
            out[i].c[axis] += delta * sign;
        }
        else if (!(flag & same_or_pos)) // if same, don't add delta
        {
            int16_t delta = read_16(reader);
            out[i].c[axis] += delta;
        }
    }
}

int ttf_num_points(struct ttf_glyph *glyph)
{
    if (glyph->num_contours == 0) return 0;
    return glyph->contour_endpoints[glyph->num_contours - 1] + 1;
}

static RESULT parse_simple_glyf(struct ttf_reader *reader,
                                struct ttf_glyph *glyph)
{
    uint16_t *contour_endpoints =
        malloc(sizeof(*contour_endpoints) * glyph->num_contours);

    for (int i = 0; i < glyph->num_contours; i++)
        contour_endpoints[i] = read_16(reader);

    uint16_t instruction_len = read_16(reader);
    reader->cursor += instruction_len; // skip scary bytecode for now

    uint8_t *flags = NULL;
    size_t flags_len = 0;
    size_t flags_cap = 0;

    unsigned num_points = contour_endpoints[glyph->num_contours - 1] + 1;

    while (flags_len < num_points)
    {
        uint8_t flag = read_8(reader);
        uint8_t repeats = flag & REPEAT_FLAG ? repeats = read_8(reader) : 0;
        do
        {
            if (flags_len >= flags_cap)
            {
                if (flags_cap == 0) flags_cap = 2;
                flags = realloc(flags, flags_cap *= 2);
            }
            flags[flags_len++] = flag;
        } while (repeats-- > 0);
    }
    flags = realloc(flags, flags_len);

    glyph->contour_endpoints = contour_endpoints;
    glyph->points = malloc(sizeof(*glyph->points) * num_points);
    ttf_parse_coordinates(reader, flags, flags_len, X, glyph->points);
    ttf_parse_coordinates(reader, flags, flags_len, Y, glyph->points);

    free(flags);
    return OK;
}

static void apply_transform(float mat[2][2], contour_point_t *point)
{
    float result[2] = { 0.0, 0.0 };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            result[i] += mat[i][j] * point->c[j];

    point->c[0] = result[0] + 0.5;
    point->c[0] = result[1] + 0.5;
}

static RESULT parse_compound_glyf(struct ttf_reader *reader,
                                  struct ttf_glyph *glyph)
{
    printf("compound glyph\n");

    enum
    {
        ARG_1_AND_2_ARE_WORDS     = 0x0001,
        ARGS_ARE_XY_VALUES        = 0x0002,
        ROUND_XY_TO_GRID          = 0x0004,
        WE_HAVE_A_SCALE           = 0x0008,
        MORE_COMPONENTS           = 0x0020,
        WE_HAVE_AN_X_AND_Y_SCALE  = 0x0040,
        WE_HAVE_A_TWO_BY_TWO      = 0x0080,
        WE_HAVE_INSTRUCTIONS      = 0x0100,
        USE_MY_METRICS            = 0x0200,
        OVERLAP_COMPOUND          = 0x0400,
        SCALED_COMPONENT_OFFSET   = 0x0800,
        UNSCALED_COMPONENT_OFFSET = 0x1000,
    };

    glyph->num_contours = 0;
    int cap_contours = 0;
    glyph->contour_endpoints = NULL;
    glyph->points = NULL;
    int num_points = 0;
    int cap_points = 0;

    uint16_t flags;
    do
    {
        flags = read_16(reader);
        printf("    flags %04x. ", flags);
        uint16_t index = read_16(reader);
        uint16_t arg1, arg2;
        if (flags & ARG_1_AND_2_ARE_WORDS)
        {
            arg1 = read_16(reader);
            arg2 = read_16(reader);
        }
        else
        {
            uint16_t both = read_16(reader);
            arg1 = both >> 8;
            arg2 = both & 0xff;
        }

        float matrix[2][2] = { { 1.0, 0.0 },
                               { 0.0, 1.0 } };
        if (flags & WE_HAVE_A_SCALE)
        {
            float scale = read_f2dot14(reader);
            matrix[0][0] = matrix[1][1] = scale;
        }
        else if (flags & WE_HAVE_AN_X_AND_Y_SCALE)
        {
            matrix[0][0] = read_f2dot14(reader);
            matrix[1][1] = read_f2dot14(reader);
        }
        else if (flags & WE_HAVE_A_TWO_BY_TWO)
        {
            // Not a mistake, I'm transposing the matrix on purpose.
            // The file stores it in column-major form, C is row-major
            matrix[0][0] = read_f2dot14(reader);
            matrix[1][0] = read_f2dot14(reader);
            matrix[0][1] = read_f2dot14(reader);
            matrix[1][1] = read_f2dot14(reader);
        }

        struct ttf_glyph child;
        if (ttf_parse_glyf(reader, index, &child)) goto fail;

        int child_np = ttf_num_points(&child);
        printf("child %d has %d points and %d contours\n",
               index,
               child_np,
               child.num_contours);
        for (int i = 0; i < child.num_contours; i++)
        {
            if (glyph->num_contours >= cap_contours)
            {
                if (cap_contours == 0) cap_contours = 2;
                cap_contours *= 2;
                glyph->contour_endpoints =
                    realloc(glyph->contour_endpoints,
                            sizeof(*glyph->contour_endpoints) * cap_contours);
            }
            glyph->contour_endpoints[glyph->num_contours++] =
                child.contour_endpoints[i] + num_points;
        }

        if ((flags & ARGS_ARE_XY_VALUES) == 0)
            error(1, 0, "TODO: contour point offset weirdness");

        for (int i = 0; i < child_np; i++)
        {
            if (num_points >= cap_points)
            {
                if (cap_points == 0) cap_points = 2;
                cap_points *= 2;
                glyph->points =
                    realloc(glyph->points, sizeof(*glyph->points) * cap_points);
            }
            glyph->points[num_points] = child.points[i];
            if (flags & SCALED_COMPONENT_OFFSET)
            {
                glyph->points[num_points].c[0] += (int16_t) arg1;
                glyph->points[num_points].c[1] += (int16_t) arg2;
                apply_transform(matrix, &glyph->points[num_points]);
            }
            else
            {
                apply_transform(matrix, &glyph->points[num_points]);
                glyph->points[num_points].c[0] += (int16_t) arg1;
                glyph->points[num_points].c[1] += (int16_t) arg2;
            }
            num_points++;
        }

        free(child.contour_endpoints);
        free(child.points);
    } while (flags & MORE_COMPONENTS);

    glyph->contour_endpoints =
        realloc(glyph->contour_endpoints,
                sizeof(*glyph->contour_endpoints) * glyph->num_contours);
    glyph->points = realloc(glyph->points, sizeof(*glyph->points) * num_points);

    return OK;

    /*
    if (flags & WE_HAVE_INSTRUCTIONS)
    {
        uint16_t num_instr = read_16();
        reader->cursor += num_instr;
    }
    */
fail:
    free(glyph->contour_endpoints);
    free(glyph->points);
    return ERR;
}

RESULT ttf_parse_glyf(struct ttf_reader *reader,
                      uint16_t index,
                      struct ttf_glyph *glyph)
{
    int offset = reader->locations[index];
    int next_offset = reader->locations[index + 1];
    if (offset == next_offset) // empty glyph
    {
        glyph->num_contours = 0;
        glyph->bbox.x_min = 0;
        glyph->bbox.y_min = 0;
        glyph->bbox.x_max = 0;
        glyph->bbox.y_max = 0;
        glyph->points = NULL;
        glyph->contour_endpoints = NULL;
        return OK;
    }

    void *old_cursor = reader->cursor;
    reader->cursor = reader->glyphs + offset;

    glyph->num_contours = read_16(reader);
    glyph->bbox = read_bbox(reader);

    if (glyph->num_contours < 0)
    {
        if (parse_compound_glyf(reader, glyph)) goto fail;
    }
    else
    {
        if (parse_simple_glyf(reader, glyph)) goto fail;
    }

    reader->cursor = old_cursor;
    return OK;

fail:
    reader->cursor = old_cursor;
    return ERR;
}
