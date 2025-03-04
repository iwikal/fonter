#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "truetype.h"
#include "shortmap.h"

#ifndef READALL_CHUNK
#define READALL_CHUNK 4096
#endif

size_t readall(FILE *f, uint8_t **out);
void bmp_to_utf8(uint16_t c, char *s);
uint16_t utf8_codepoint(const char *c);
bool utf8_continuation(char c);
int utf8_codepoint_len(char c);
int check_status(unsigned shader);
unsigned compile_shader(const char *path, GLenum type);
unsigned shader_program(const char *vert_source, const char *frag_source);
void debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                    GLsizei length, const GLchar *message, const void *param);
unsigned generate_glyph_mesh(struct ttf_glyph *glyph, unsigned textures[2]);
void destroy_glyph_mesh(unsigned vao);


int main(int argc, char *argv[])
{
    GLFWwindow *window;
    if (!glfwInit()) error(ERR, 0, "Failed to init GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_DEBUG, true);
    glfwWindowHint(GLFW_RESIZABLE, false);
    int width = 800, height = 640;
    window = glfwCreateWindow(width, height, "Fonter", NULL, NULL);
    if (!window) error(ERR, 0, "Failed to init window");

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        error(ERR, 0, "Failed to init GLAD");

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
    glDebugMessageCallback(debug_callback, NULL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(1.0, 1.0, 1.0, 1.0);

    if (argc < 2)
    {
        argc = 2;
        argv[1] = "/usr/share/fonts/noto/NotoSerifDevanagari-Regular.ttf";
    }

    struct ttf_reader reader;
    {
        FILE *fontfile = fopen(argv[1], "rb");
        if (readall(fontfile, (uint8_t **)&reader.data) == ERR)
            error(ERR, errno, "Failed to read %s", argv[1]);
        fclose(fontfile);

        reader.cursor = reader.data;
        if (ttf_parse(&reader) == ERR)
            error(ERR, 0, "Failed to parse ttf %s", argv[1]);

        printf("num glyphs: %d\n", reader.num_glyphs);
    }

    // FIXME devanagari support: uint16_t c = utf8_codepoint("अ");
    // FIXME wtf:                uint16_t c = utf8_codepoint("h");
    // FIXME wtf:                uint16_t c = utf8_codepoint("k");
    // uint16_t c = utf8_codepoint("g");

    unsigned shader = shader_program("quad.glsl", "sdf.glsl");

    int u_dims = glGetUniformLocation(shader, "u_dims");
    int u_pos = glGetUniformLocation(shader, "u_pos");
    int u_points = glGetUniformLocation(shader, "u_points");
    int u_endpoints = glGetUniformLocation(shader, "endpoints");
    int u_num_contours = glGetUniformLocation(shader, "num_contours");
    int u_num_points = glGetUniformLocation(shader, "num_points");
    int u_units_per_em = glGetUniformLocation(shader, "units_per_em");
    int u_size = glGetUniformLocation(shader, "u_size");
    int u_bbox_min = glGetUniformLocation(shader, "u_bbox_min");
    int u_bbox_max = glGetUniformLocation(shader, "u_bbox_max");

    const char message[] = "बकवास";

    struct glyph_mesh
    {
        uint16_t id;
        struct ttf_glyph glyph;
        unsigned vao;
        unsigned textures[2];
    };

    shortmap_t meshes = shortmap_create(16);

    for (int i = 0, n; n = utf8_codepoint_len(message[i]), message[i] != 0; i += n)
    {
        char s[5] = {};
        for (int j = 0; j < n; j++)
            s[j] = message[i + j];

        uint16_t c = utf8_codepoint(&message[i]);
        uint16_t glyph_id = ttf_lookup_index(reader.cmap, c);

        // skip if we've already generated this mesh
        if (shortmap_get(&meshes, glyph_id) != NULL) continue;

        struct glyph_mesh *mesh = malloc(sizeof(*mesh));
        mesh->id = glyph_id;
        if (ttf_parse_glyf(&reader, glyph_id, &mesh->glyph) != OK)
            error(1, 0, "failed to parse glyf %d (%s)", glyph_id, s);

        mesh->vao = generate_glyph_mesh(&mesh->glyph, mesh->textures);

        shortmap_insert(&meshes, glyph_id, mesh);
    }

    bool has_drawn = false;
    while (!glfwWindowShouldClose(window))
    {
        if (!has_drawn)
        {
            glClear(GL_COLOR_BUFFER_BIT);

            float fontsize = 24.0;
            glUseProgram(shader);
            glUniform1f(u_units_per_em, (float)reader.units_per_em);
            glUniform1f(u_size, fontsize);
            glUniform2i(u_dims, width, height);

            float xpos = reader.units_per_em,
                  ypos = 26900 / 2;

            for (int i = 0, n; n = utf8_codepoint_len(message[i]), message[i] != 0; i += n)
            {
                uint16_t c = utf8_codepoint(&message[i]);
                uint16_t glyph_id = ttf_lookup_index(reader.cmap, c);

                struct glyph_mesh *mesh = shortmap_get(&meshes, glyph_id);

                if (mesh->glyph.num_contours > 0)
                {
                    for (int t = 0; t < 2; t++)
                    {
                        glActiveTexture(GL_TEXTURE0 + t);
                        glBindTexture(GL_TEXTURE_BUFFER, mesh->textures[t]);
                    }
                    glUniform1i(u_points, 0);
                    glUniform1i(u_endpoints, 1);

                    glBindVertexArray(mesh->vao);
                    glUniform2f(u_pos, xpos, ypos);
                    glUniform1ui(u_num_contours, mesh->glyph.num_contours);
                    glUniform1ui(u_num_points, ttf_num_points(&mesh->glyph));
                    glUniform2i(u_bbox_min, mesh->glyph.bbox.x_min, mesh->glyph.bbox.y_min);
                    glUniform2i(u_bbox_max, mesh->glyph.bbox.x_max, mesh->glyph.bbox.y_max);

                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }

                float advance = reader.hmetrics[mesh->id].advance_width;
                xpos += advance;
            }

            glBindVertexArray(0);
            glUseProgram(0);

            glfwSwapBuffers(window);
            // has_drawn = true;
        }

        glfwWaitEvents();
    }

    return 0;
}

size_t readall(FILE *f, uint8_t **out)
{
    uint8_t *temp;
    size_t size = 0, used = 0;

    if (f == NULL || out == NULL || ferror(f))
        return ERR;

    *out = NULL;

    while (1)
    {
        if (used + READALL_CHUNK > size)
        {
            size = used + READALL_CHUNK;

            if (size <= used) // overflow check
            {
                free(*out);
                return ERR;
            }

            temp = realloc(*out, size);

            if (temp == NULL) // OOM check
            {
                free(*out);
                return ERR;
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
        return ERR;
    }

    temp = realloc(*out, used);
    if (temp == NULL)
    {
        free(*out);
        return ERR;
    }
    *out = temp;

    return used;
}

void bmp_to_utf8(uint16_t c, char *s)
{
    if (c <= 0x7F) {
        // 1-byte UTF-8
        s[0] = (char)c;
        s[1] = '\0';
    } else if (c <= 0x7FF) {
        // 2-byte UTF-8
        s[0] = (char)((c >> 6) | 0xC0);
        s[1] = (char)((c & 0x3F) | 0x80);
        s[2] = '\0';
    } else {
        // 3-byte UTF-8
        s[0] = (char)((c >> 12) | 0xE0);
        s[1] = (char)(((c >> 6) & 0x3F) | 0x80);
        s[2] = (char)((c & 0x3F) | 0x80);
        s[3] = '\0';
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

bool utf8_continuation(char c)
{
    return (c & 0xc0) == 0x80;
}

int utf8_codepoint_len(char c)
{
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xe0) == 0xc0) return 2;
    if ((c & 0xf0) == 0xe0) return 3;
    if ((c & 0xf8) == 0xf0) return 4;
    return 1;
}

int check_status(unsigned shader)
{
    int success, size = 0;
    bool s = glIsShader(shader);

    if (s) glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    else glGetProgramiv(shader, GL_LINK_STATUS, &success);

    if (!success)
    {
        if (s) glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
        else glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &size);
        char infolog[size];
        if (s) glGetShaderInfoLog(shader, size, NULL, infolog);
        else glGetProgramInfoLog(shader, size, NULL, infolog);
        fprintf(stderr, "Shader compilation failed:\n%s", infolog);
        return ERR;
    }

    return OK;
}

unsigned compile_shader(const char *path, GLenum type)
{
    unsigned shader = glCreateShader(type);
    FILE *f = fopen(path, "r");

    uint8_t *source = NULL;
    int size = readall(f, &source);
    if (size == ERR)
        error(1, errno, "Failed to read %s", path);

    fclose(f);

    glShaderSource(shader, 1, (const char * const*)&source, &size);
    free(source);
    glCompileShader(shader);

    if (check_status(shader) == ERR) return 0;

    return shader;
}

unsigned shader_program(const char *vert_source, const char *frag_source)
{
    unsigned vs = compile_shader("quad.glsl", GL_VERTEX_SHADER);
    if (vs == 0) return ERR;
    unsigned fs = compile_shader("sdf.glsl", GL_FRAGMENT_SHADER);
    if (fs == 0) return ERR;

    unsigned program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (check_status(program) == ERR) return 0;

    return program;
}

void debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                   GLsizei length, const GLchar *message, const void *param)
{
    error(0, 0, "%s", message);
}

unsigned generate_glyph_mesh(struct ttf_glyph *glyph,
                             unsigned textures[2])
{
    unsigned points = 0, endpoints = 0, vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // FIXME why is this necessary???
    // NOTE: it might have something to do with certain control points being on
    // top of others, probably creating singularities. But why?
    for (int i = 0, n = ttf_num_points(glyph); i < n; i++)
    {
        glyph->points[i].c[0] += i % 2;
        glyph->points[i].c[1] += (i / 2) % 2;
    }

    glGenBuffers(1, &points);
    glBindBuffer(GL_TEXTURE_BUFFER, points);
    glBufferData(GL_TEXTURE_BUFFER,
                 sizeof(contour_point_t) * ttf_num_points(glyph),
                 glyph->points,
                 GL_STREAM_DRAW);

    glGenBuffers(1, &endpoints);
    glBindBuffer(GL_TEXTURE_BUFFER, endpoints);
    glBufferData(GL_TEXTURE_BUFFER,
                 sizeof(uint16_t) * glyph->num_contours,
                 glyph->contour_endpoints,
                 GL_STREAM_DRAW);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    glGenTextures(2, textures);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, textures[0]);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32I, points);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, textures[1]);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R16UI, endpoints);

    glBindVertexArray(0);

    return vao;
}

void destroy_glyph_mesh(unsigned vao)
{
    glBindVertexArray(vao);

    int tex[2];
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &tex[0]);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &tex[1]);
    glDeleteTextures(2, (unsigned *)tex);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}
