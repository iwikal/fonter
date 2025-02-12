#include <stdlib.h>
#include <stdint.h>
#include <endian.h>
#include <stdbool.h>
#include <stdio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "truetype.h"

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

void *bmp_to_utf8(uint16_t c, char *utf8_str) {
    if (c <= 0x7F) {
        // 1-byte UTF-8
        utf8_str[0] = (char)c;
        utf8_str[1] = '\0';
    } else if (c <= 0x7FF) {
        // 2-byte UTF-8
        utf8_str[0] = (char)((c >> 6) | 0xC0);
        utf8_str[1] = (char)((c & 0x3F) | 0x80);
        utf8_str[2] = '\0';
    } else {
        // 3-byte UTF-8
        utf8_str[0] = (char)((c >> 12) | 0xE0);
        utf8_str[1] = (char)(((c >> 6) & 0x3F) | 0x80);
        utf8_str[2] = (char)((c & 0x3F) | 0x80);
        utf8_str[3] = '\0';
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

    struct ttf_reader reader = { .data = data, .cursor = data };
    {
        int result = ttf_parse(&reader);
        printf("result: %d\n", result);
        printf("num glyphs: %d\n", reader.num_glyphs);
    }

    // FIXME devanagari support: uint16_t c = utf8_codepoint("अ");
    // FIXME wtf:                uint16_t c = utf8_codepoint("h");
    // FIXME wtf:                uint16_t c = utf8_codepoint("k");
    uint16_t c = 0x48b; //utf8_codepoint("ɛ");
    printf("U+%x\n", c);

    struct ttf_glyph glyph;
    {
        uint16_t index = ttf_lookup_index(reader.cmap, c);

        ttf_parse_glyf(&reader, index, &glyph);
        printf("index %d\n", index);
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
                 sizeof(contour_point_t) * ttf_num_points(&glyph),
                 glyph.points,
                 GL_STATIC_DRAW);

    glGenBuffers(1, &endpoints);
    glBindBuffer(GL_TEXTURE_BUFFER, endpoints);
    glBufferData(GL_TEXTURE_BUFFER,
                 sizeof(uint16_t) * glyph.num_contours,
                 glyph.contour_endpoints,
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

    bool has_drawn = false;
    while (!glfwWindowShouldClose(window))
    {
        if (!has_drawn)
        {
            glClear(GL_COLOR_BUFFER_BIT);

            glBindVertexArray(vao);
            glUniform1i(u_points, 0);
            glUniform1i(u_endpoints, 1);
            glUniform1ui(u_num_contours_location, glyph.num_contours);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            glfwSwapBuffers(window);
            has_drawn = true;
        }
        glfwWaitEvents();
    }

    free(data);
    return 0;
}
