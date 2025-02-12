#include <stdlib.h>
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

RESULT ttf_parse_head(struct ttf_reader *reader, uint16_t *loc_format)
{
    if (reader->cursor == NULL)
    {
        fprintf(stderr, "no head!\n");
        return ERR;
    }

    read_32(reader); // skip version
    read_32(reader); // skip font revision
    read_32(reader); // skip checksum adjustment
    read_32(reader); // skip magic number
    read_16(reader); // skip flags
    read_16(reader); // skip units per em

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
    if (reader->locations == NULL)
    {
        fprintf(stderr, "out of memory\n");
        return ERR;
    }

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

    size_t length = read_16(reader); // skip length
    read_16(reader); // skip language
    uint16_t seg_count = read_16(reader) / 2;
    read_16(reader); // skip search range
    read_16(reader); // skip entry selector
    read_16(reader); // skip range shift

    size_t tail_len = subtable + length - reader->cursor;
    struct cmap_4 *sub = malloc(sizeof(struct cmap_4) + tail_len);
    if (sub == NULL)
    {
        fprintf(stderr, "out of memory\n");
        return ERR;
    }

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

    void *head = NULL, *maxp = NULL, *hhea = NULL,
         *cmap = NULL, *loca = NULL, *glyf = NULL;

    for (int i = 0; i < num_tables; i++)
    {
        uint32_t tag      = read_32(reader);
        uint32_t checksum = read_32(reader);
        uint32_t offset   = read_32(reader);
        /* ignore length */ read_32(reader);

        void *table = reader->data + offset;
        switch (tag)
        {
            case TAG_head: head = table; break;
            case TAG_maxp: maxp = table; break;
            case TAG_hhea: hhea = table; break;
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

    reader->glyphs = glyf;

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
    return glyph->contour_endpoints[glyph->num_contours - 1] + 1;
}

RESULT ttf_parse_glyf(struct ttf_reader *reader,
                      uint16_t index,
                      struct ttf_glyph *glyph)
{
    int offset = reader->locations[index];
    void *old_cursor = reader->cursor;
    reader->cursor = reader->glyphs + offset;

    int16_t num_contours = read_16(reader);
    if (num_contours < 0)
    {
        fprintf(stderr, "WARNING: Compound glyph!\n");
        reader->cursor = old_cursor;
        return ERR;
    }

    glyph->num_contours = num_contours;
    glyph->bbox = read_bbox(reader);
    
    printf("simple! %d contours.\n", num_contours);

    uint16_t *contour_endpoints = malloc(sizeof(*contour_endpoints) * num_contours);
    for (int i = 0; i < num_contours; i++)
        contour_endpoints[i] = read_16(reader);

    uint16_t instruction_len = read_16(reader);
    reader->cursor += instruction_len; // skip scary bytecode for now

    uint8_t *flags = NULL;
    size_t flags_len = 0;
    size_t flags_cap = 0;

    unsigned num_points = contour_endpoints[num_contours - 1] + 1;
    printf("%d points.\n", num_points);

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

    reader->cursor = old_cursor;
    return OK;
}
