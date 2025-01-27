#include <stdlib.h>
#include <stdint.h>
#include <endian.h>
#include <stdbool.h>
#include <stdio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

typedef uint32_t Fixed;
typedef uint64_t Date;
typedef int16_t FWord;
typedef uint16_t UFWord;

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
  TAG_cmap = 0x70616d63,
  TAG_cvt  = 0x20747663,
  TAG_fpgm = 0x6d677066,
  TAG_gasp = 0x70736167,
  TAG_gdef = 0x46454447,
  TAG_glyf = 0x66796c67,
  TAG_gpos = 0x534f5047,
  TAG_gsub = 0x42555347,
  TAG_head = 0x64616568,
  TAG_hhea = 0x61656868,
  TAG_hmtx = 0x78746d68,
  TAG_loca = 0x61636f6c,
  TAG_maxp = 0x7078616d,
  TAG_name = 0x656d616e,
  TAG_os_2 = 0x322f534f,
  TAG_otto = 0x4f54544f,
  TAG_post = 0x74736f70,
  TAG_prep = 0x70657270,
  TAG_truetype = 0x00000100,
  TAG_truetypecollection = 0x66637474,
};

typedef struct
{
  uint32_t tableTag;
  uint32_t checksum;
  uint32_t offset;
  uint32_t length;
} TableRecord;

struct table_dir
{
  uint32_t sfntVersion;
  uint16_t numTables;
  uint16_t searchRange;
  uint16_t entrySelector;
  uint16_t rangeShift;
  TableRecord records[];
};

// The dates, which are 64 bits wide, necessitate packing
struct __attribute__((packed)) head
{
  uint16_t majorVersion;
  uint16_t minorVersion;
  Fixed fontRevision;
  uint32_t checksumAdjustment;
  uint32_t magicNumber;
  uint16_t flags;
  uint16_t unitsPerEm;
  Date created;
  Date modified;
  FWord xMin;
  FWord yMin;
  FWord xMax;
  FWord yMax;
  uint16_t macStyle;
  uint16_t lowestRecPPEM;
  uint16_t fontDirectionHint;
  uint16_t indexToLocFormat;
  uint16_t glyphDataFormat;
};

struct maxp
{
  Fixed version;
  uint16_t numGlyphs;
  /*
  // the rest is not needed
  uint16_t maxPoints;
  uint16_t maxContours;
  uint16_t maxCompositePoints;
  uint16_t maxCompositeContours;
  uint16_t maxZones;
  uint16_t maxTwilightPoints;
  uint16_t maxStorage;
  uint16_t maxFunctionDefs;
  uint16_t maxInstructionDefs;
  uint16_t maxStackElements;
  uint16_t maxSizeOfInstructions;
  uint16_t maxComponentElements;
  uint16_t maxComponentDepth;
  */
};

struct hhea
{
  uint16_t majorVersion;
  uint16_t minorVersion;
  FWord ascender;
  FWord descender;
  FWord lineGap;
  UFWord advanceWidthMax;
  FWord minLeftSideBearing;
  FWord minRightSideBearing;
  FWord xMaxExtent;
  int16_t caretSlopeRise;
  int16_t caretSlopeRun;
  int16_t caretOffset;
  int16_t reserved[4];
  int16_t metricDataFormat;
  uint16_t numberOfHMetrics;
};

typedef struct
{
  uint16_t platformID;
  uint16_t encodingID;
  uint32_t subtableOffset;
} EncodingRecord;

struct cmap
{
  uint16_t version;
  uint16_t numTables;
  EncodingRecord records[];
};

struct format4
{
  uint16_t format; // should be 4
  uint16_t length;
  uint16_t language;
  uint16_t segCountX2;
  uint16_t searchRange;
  uint16_t entrySelector;
  uint16_t rangeShift;
  uint16_t tail[];
};

struct glyf
{
  int16_t numberOfContours;
  int16_t xMin;
  int16_t yMin;
  int16_t xMax;
  int16_t yMax;
  uint8_t tail[];
};

#ifndef READALL_CHUNK
#define READALL_CHUNK 0x40000000 // 1 MB
#endif

size_t readall(FILE *f, uint8_t **out)
{
  uint8_t *temp;
  size_t size = 0, used = 0;

  if (f == NULL || out == NULL || ferror(f))
    return -1;

  *out = NULL;

  while (1)
  {
    if (used + READALL_CHUNK > size)
    {
      size = used + READALL_CHUNK;

      if (size <= used) // overflow check
      {
        free(*out);
        return -1;
      }

      temp = realloc(*out, size);

      if (temp == NULL) // OOM check
      {
        free(*out);
        return -1;
      }

      *out = temp;
    }

    size_t n = fread(*out + used, 1, READALL_CHUNK, f);
    if (n == 0) break;

    used += n;
  }

  if (ferror(f))
  {
    free(*out);
    return -1;
  }

  temp = realloc(*out, used);
  if (temp == NULL)
  {
    free(*out);
    return -1;
  }
  *out = temp;

  return size;
}

void parse_head(struct head *head)
{
  head->majorVersion = be16toh(head->majorVersion);
  head->minorVersion = be16toh(head->minorVersion);
  head->fontRevision = be32toh(head->fontRevision);
  head->checksumAdjustment = be32toh(head->checksumAdjustment);
  head->magicNumber = be32toh(head->magicNumber);
  head->flags = be16toh(head->flags);
  head->unitsPerEm = be16toh(head->unitsPerEm);
  head->created = be64toh(head->created);
  head->modified = be64toh(head->modified);
  head->xMin = be16toh(head->xMin);
  head->yMin = be16toh(head->yMin);
  head->xMax = be16toh(head->xMax);
  head->yMax = be16toh(head->yMax);
  head->macStyle = be16toh(head->macStyle);
  head->lowestRecPPEM = be16toh(head->lowestRecPPEM);
  head->fontDirectionHint = be16toh(head->fontDirectionHint);
  head->indexToLocFormat = be16toh(head->indexToLocFormat);
  head->glyphDataFormat = be16toh(head->glyphDataFormat);
}

void parse_maxp(struct maxp *maxp)
{
  maxp->version = be32toh(maxp->version);
  maxp->numGlyphs = be16toh(maxp->numGlyphs);
}

void parse_hhea(struct hhea *hhea)
{
  hhea->numberOfHMetrics = be16toh(hhea->numberOfHMetrics);
}

uint32_t *parse_loca(uint16_t *loca, uint16_t numGlyphs, uint16_t indexToLocFormat)
{
  uint16_t *shortform = loca;
  uint32_t *longform = (void *)loca;

  uint32_t *locations = malloc(sizeof(*locations) * (numGlyphs + 1));

  for (int i = 0; i <= numGlyphs; i++)
  {
    if (indexToLocFormat == 0)
      locations[i] = be16toh(shortform[i]) * 2;
    else
      locations[i] = be32toh(longform[i]);
  }

  return locations;
}

void format4_arrays(struct format4 *sub,
                    uint16_t **endCode,
                    uint16_t **startCode,
                    uint16_t **idDelta,
                    uint16_t **idRangeOffset,
                    uint16_t **glyphIdArray)
{
  uint16_t segCount = sub->segCountX2 / 2;
  void *ptr = sub->tail;
  *endCode = ptr;
  ptr += sizeof(**endCode) * segCount;
  ptr += sizeof(uint16_t); // reserved padding

  *startCode = ptr;
  ptr += sizeof(**startCode) * segCount;

  *idDelta = ptr;
  ptr += sizeof(**idDelta) * segCount;

  *idRangeOffset = ptr;
  ptr += sizeof(**idRangeOffset) * segCount;

  *glyphIdArray = ptr;
}

void parse_format4(struct format4 *sub)
{
  sub->format = be16toh(sub->format);
  if (sub->format != 4)
      fprintf(stderr, "WARNING: unexpected format %d\n", sub->format);

  sub->length = be16toh(sub->length);
  sub->language = be16toh(sub->language);
  sub->segCountX2 = be16toh(sub->segCountX2);
  sub->searchRange = be16toh(sub->searchRange);
  sub->entrySelector = be16toh(sub->entrySelector);
  sub->rangeShift = be16toh(sub->rangeShift);

  uint16_t *endCode, *startCode, *idDelta, *idRangeOffset, *glyphIdArray;
  format4_arrays(sub, &endCode, &startCode, &idDelta, &idRangeOffset, &glyphIdArray);

  uint16_t segCount = sub->segCountX2 / 2;
  for (int i = 0; i < segCount; i++)
  {
    endCode[i] = be16toh(endCode[i]);
    startCode[i] = be16toh(startCode[i]);
    idDelta[i] = be16toh(idDelta[i]);
    idRangeOffset[i] = be16toh(idRangeOffset[i]);
    glyphIdArray[i] = be16toh(glyphIdArray[i]);
  }
}

struct format4 *parse_cmap(struct cmap *cmap)
{
  cmap->version = be16toh(cmap->version);
  cmap->numTables = be16toh(cmap->numTables);

  struct format4 *subtable = NULL;
  for (int i = 0; i < cmap->numTables; i++)
  {
    EncodingRecord *rec = &cmap->records[i];
    rec->platformID = be16toh(rec->platformID);
    rec->encodingID = be16toh(rec->encodingID);
    rec->subtableOffset = be32toh(rec->subtableOffset);
    if (rec->platformID == 0 && rec->encodingID == 3)
    {
      subtable = (void *)cmap + rec->subtableOffset;
      break;
    }
  }

  if (subtable == NULL) return NULL;
  parse_format4(subtable);

  return subtable;
}

uint16_t lookup_index(struct format4 *sub, uint16_t c)
{
  uint16_t segCount = sub->segCountX2 / 2;
  uint16_t *endCode, *startCode, *idDelta, *idRangeOffset, *glyphIdArray;
  format4_arrays(sub, &endCode, &startCode, &idDelta, &idRangeOffset, &glyphIdArray);
  for (int i = 0; i < segCount; i++)
  {
    if (endCode[i] >= c)
    {
      if (c < startCode[i]) goto fail;

      if (idRangeOffset[i] == 0) return idDelta[i] + c;

      return be16toh(*(idRangeOffset[i]/2
          + (c - startCode[i])
          + &idRangeOffset[i]));
    }
  }

fail:
  return 0;
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
typedef struct
{
  int c[2];
  int on_curve;
} point_t;

void parse_coordinates(void **cursor,
                       uint8_t *flags,
                       size_t flags_len,
                       int axis,
                       point_t *out)
{
  uint8_t short_vector = axis == X
    ? X_SHORT_VECTOR
    : Y_SHORT_VECTOR;
  uint8_t same_or_pos = axis == X
    ? X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR
    : Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR;

  for (int i = 0; i < flags_len; i++)
  {
    out[i].c[axis] = i == 0 ? 0 : out[i - 1].c[axis];

    out[i].on_curve = (flags[i] & ON_CURVE_POINT) != 0;

    if (flags[i] & short_vector)
    {
      uint8_t delta = *(uint8_t *)(*cursor);
      *cursor += sizeof(delta);
      int sign = flags[i] & same_or_pos ? 1 : -1;
      out[i].c[axis] += delta * sign;
    }
    else if (!(flags[i] & same_or_pos)) // if same, don't add delta
    {
      int16_t delta = be16toh(*(int16_t *)(*cursor));
      *cursor += sizeof(delta);
      out[i].c[axis] += delta;
    }
    uint8_t flag = flags[i];
  }
}

struct simple_glyph
{
  point_t *points;
  uint16_t *contour_endpoints;
};

void free_simple_glyph(struct simple_glyph *simple)
{
  free(simple->points);
  free(simple->contour_endpoints);
}

struct compound_glyph
{
};

struct glyph
{
  int16_t num_contours;
  int16_t xMin;
  int16_t yMin;
  int16_t xMax;
  int16_t yMax;
  union
  {
    struct simple_glyph simple;
    struct simple_glyph compound;
  } ty;
};

void free_glyph(struct glyph *glyph)
{
  if (glyph->num_contours < 0)
  {
    // TODO free compound
  }
  else
  {
    free_simple_glyph(&glyph->ty.simple);
  }
}

int glyph_num_points(struct glyph *glyph)
{
    uint16_t *endpoints = glyph->ty.simple.contour_endpoints;
    return endpoints[glyph->num_contours - 1] + 1;
}

void parse_simple_glyph(struct glyf *glyf, struct simple_glyph *out)
{
  int16_t num_contours = be16toh(glyf->numberOfContours);
  printf("simple! %d contours.\n", num_contours);

  void *cursor = glyf->tail;

  uint16_t *contour_endpoints = malloc(sizeof(*contour_endpoints) * num_contours);;
  for (int i = 0; i < num_contours; i++)
  {
    contour_endpoints[i] = be16toh(*(uint16_t *)cursor);
    cursor += sizeof(uint16_t);
  }

  uint16_t instructionLength = be16toh(*(uint16_t *)cursor);
  cursor += sizeof(instructionLength);

  cursor += sizeof(uint8_t) * instructionLength; // skip scary bytecode for now

  uint8_t *flags = NULL;
  size_t flags_len = 0;
  size_t flags_cap = 0;

  unsigned num_points = contour_endpoints[num_contours - 1] + 1;
  printf("%d points.\n", num_points);

  uint8_t first_flag = *(uint8_t *)cursor;
  if (first_flag & OVERLAP_SIMPLE)
      fprintf(stderr, "WARNING! Overlapping contours. Expect bugs.\n");

  while (flags_len < num_points)
  {
    uint8_t flag = *(uint8_t *)(cursor++);
    uint8_t repeats = flag & REPEAT_FLAG ? repeats = *(uint8_t *)(cursor++) : 0;
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

  out->contour_endpoints = contour_endpoints;
  out->points = malloc(sizeof(*out->points) * num_points);
  parse_coordinates(&cursor, flags, flags_len, X, out->points);
  parse_coordinates(&cursor, flags, flags_len, Y, out->points);

  free(flags);
}

void parse_glyf(struct glyf *glyf, struct glyph *out)
{
  out->num_contours = be16toh(glyf->numberOfContours);
  out->xMin = be16toh(glyf->xMin);
  out->yMin = be16toh(glyf->yMin);
  out->xMax = be16toh(glyf->xMax);
  out->yMax = be16toh(glyf->yMax);

  if (out->num_contours < 0)
  {
    fprintf(stderr, "WARNING: Compound glyph!\n");
    // TODO compound glyph
  }
  else
  {
    parse_simple_glyph(glyf, &out->ty.simple);
  }
}

uint16_t utf8_codepoint(const char *c)
{
  uint16_t codepoint = c[0];

  if ((c[0] & 0x80) == 0)
    return codepoint;

  if ((c[0] & 0xe0) == 0xc0)
  {
    codepoint &= 0x1f;
    codepoint <<= 6;
    codepoint |= c[1] & 0x3f;
    // if (codepoint < 0x80) return 0xfffd;
    return codepoint;
  }

  if ((c[0] & 0xf0) == 0xe0)
  {
    codepoint &= 0x0f;
    codepoint <<= 6; codepoint |= c[1] & 0x3f;
    codepoint <<= 6; codepoint |= c[2] & 0x3f;
    // if (codepoint < 0x800) return 0xfffd;
    return codepoint;
  }

  if ((c[0] & 0xf8) == 0xf0)
  {
    codepoint &= 0x07;
    codepoint <<= 6; codepoint |= c[1] & 0x3f;
    codepoint <<= 6; codepoint |= c[2] & 0x3f;
    codepoint <<= 6; codepoint |= c[3] & 0x3f;
    return 0xfffd;
  }

  return 0xfffd;
}

int main(int argc, char *argv[])
{
  GLFWwindow *window;
  if (!glfwInit())
    return -1;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, false);
  window = glfwCreateWindow(800, 640, "Fonter", NULL, NULL);
  if (!window)
  {
    fprintf(stderr, "%s: failed to init window\n", argv[0]);
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    printf("failed to init GLAD\n");
    return -1;
  }

  argc = 2;
  argv[1] = "/usr/share/fonts/noto/NotoSerif-Regular.ttf";

  if (argc < 2)
  {
    fprintf(stderr, "usage: %s fontfile.ttf\n", argv[0]);
    glfwTerminate();
    return 1;
  }

  FILE *fontfile = fopen(argv[1], "rb");

  uint8_t *data = NULL;
  {
    size_t all_size = readall(fontfile, &data);
    if (all_size == -1)
    {
      fprintf(stderr, "%s: failed to read file\n", argv[0]);
      return -1;
    }
  }

  struct table_dir *dir = (void *)data;
  // check version tag
  switch (dir->sfntVersion)
  {
    case TAG_truetype:
      break;
    case TAG_truetypecollection:
      printf("TTC not supported\n");
      return -1;
    default:
      fprintf(stderr, "not an otf file: %32x\n", dir->sfntVersion);
      return -1;
  }

  // convert fields to native endian
  dir->numTables = be16toh(dir->numTables);
  dir->searchRange = be16toh(dir->searchRange);
  dir->entrySelector = be16toh(dir->entrySelector);
  dir->rangeShift = be16toh(dir->rangeShift);

  struct head *head = NULL;
  struct maxp *maxp = NULL;
  struct hhea *hhea = NULL;
  struct cmap *cmap = NULL;
  uint16_t *loca = NULL;
  struct glyf *glyf = NULL;

  // convert fields in each table record to native endian
  for (size_t i = 0; i < dir->numTables; i++)
  {
    TableRecord *rec = &dir->records[i];
    rec->checksum = be32toh(rec->checksum);
    rec->offset = be32toh(rec->offset);
    rec->length = be32toh(rec->length);

    void *table = data + rec->offset;
    switch (rec->tableTag)
    {
      case TAG_head: head = table; break;
      case TAG_maxp: maxp = table; break;
      case TAG_hhea: hhea = table; break;
      case TAG_cmap: cmap = table; break;
      case TAG_loca: loca = table; break;
      case TAG_glyf: glyf = table; break;
      default: break; // unsupported table, ignore
    }
  }

  if (head == NULL)
  {
    fprintf(stderr, "headless font! spooky!\n");
    return -1;
  }

  parse_head(head);
  parse_maxp(maxp);
  parse_hhea(hhea);
  uint32_t *locations = parse_loca(loca, maxp->numGlyphs, head->indexToLocFormat);
  struct format4 *cmap_subtable = parse_cmap(cmap);

  // FIXME devanagari support: uint16_t c = utf8_codepoint("अ");
  uint16_t c = utf8_codepoint("ß");
  printf("U+%x\n", c);

  struct glyph glyph;
  {
    uint16_t index = lookup_index(cmap_subtable, c);
    printf("index = %d\n", index);
    uint32_t off = locations[index];
    parse_glyf(((void *)glyf) + off, &glyph);
    printf("done\n");
  }

  GLuint shader = 0;
  {
    int success;
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    uint8_t *source = NULL;
    if (readall(fopen("quad.glsl", "r"), &source) == -1)
    {
      fprintf(stderr, "%s not found\n", "quad.glsl");
      return -1;
    };

    glShaderSource(vs, 1, (const char * const*)&source, NULL);
    free(source);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
      int size = 0;
      glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &size);
      char *infolog = malloc(size);
      glGetShaderInfoLog(vs, size, NULL, infolog);
      fprintf(stderr, "shader compilation failed:\n%s", infolog);
      free(infolog);
      return -1;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (readall(fopen("sdf.glsl", "r"), &source) == -1)
    {
      fprintf(stderr, "%s not found\n", "sdf.glsl");
      return -1;
    };

    glShaderSource(fs, 1, (const char * const*)&source, NULL);
    free(source);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
      int size = 0;
      glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &size);
      char *infolog = malloc(size);
      glGetShaderInfoLog(fs, size, NULL, infolog);
      fprintf(stderr, "shader compilation failed:\n%s", infolog);
      free(infolog);
      return -1;
    }

    shader = glCreateProgram();
    glAttachShader(shader, vs);
    glAttachShader(shader, fs);
    glLinkProgram(shader);
    glGetShaderiv(fs, GL_LINK_STATUS, &success);
    if (!success)
    {
      int size = 0;
      glGetProgramiv(fs, GL_INFO_LOG_LENGTH, &size);
      char *infolog = malloc(size);
      glGetProgramInfoLog(fs, size, NULL, infolog);
      fprintf(stderr, "shader compilation failed:\n%s", infolog);
      free(infolog);
      return -1;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
  }

  float vertices[] = { -0.5f, -0.5f, 0.0f,
                        0.5f, -0.5f, 0.0f,
                        0.0f,  0.5f, 0.0f };

  unsigned points = 0, endpoints = 0, vao = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &points);
  glBindBuffer(GL_TEXTURE_BUFFER, points);
  glBufferData(GL_TEXTURE_BUFFER,
               sizeof(point_t) * glyph_num_points(&glyph),
               glyph.ty.simple.points,
               GL_STATIC_DRAW);

  glGenBuffers(1, &endpoints);
  glBindBuffer(GL_TEXTURE_BUFFER, endpoints);
  glBufferData(GL_TEXTURE_BUFFER,
               sizeof(uint16_t) * glyph.num_contours,
               glyph.ty.simple.contour_endpoints,
               GL_STATIC_DRAW);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);

  uint textures[2] = { 0, 0 };
  glGenTextures(3, textures);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_BUFFER, textures[0]);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32I, points);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_BUFFER, textures[1]);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R16UI, endpoints);

  glBindVertexArray(0);

  glUseProgram(shader);

  int u_points = glGetUniformLocation(shader, "u_points");
  int u_endpoints = glGetUniformLocation(shader, "endpoints");
  int u_num_contours_location = glGetUniformLocation(shader, "num_contours");

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(vao);
    glUniform1i(u_points, 0);
    glUniform1i(u_endpoints, 1);
    glUniform1ui(u_num_contours_location, glyph.num_contours);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  free_glyph(&glyph);
  free(locations);
  free(data);
  return 0;
}
