#include <stdlib.h>
#include <stdint.h>
#include <endian.h>
#include <stdbool.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
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

void *bmp_to_utf8(uint16_t c, char *s) {
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

int checkStatus(GLuint shader)
{
    int success, size = 0;
    bool b = glIsShader(shader);

    if (b) glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    else glGetProgramiv(shader, GL_LINK_STATUS, &success);

    if (!success)
    {
        (b ? glGetShaderiv : glGetProgramiv)(shader, GL_INFO_LOG_LENGTH, &size);
        char *infolog = malloc(size);
        (b ? glGetShaderInfoLog : glGetProgramInfoLog)(shader, size, NULL, infolog);
        fprintf(stderr, "Shader compilation failed:\n%s", infolog);
        free(infolog);
        return ERR;
    }

    return OK;
}

GLuint compileShader(const char *path, GLenum type)
{
    GLuint shader = glCreateShader(type);
    FILE *f = fopen(path, "r");

    uint8_t *source = NULL;
    if (readall(f, &source) == -1)
        error(1, errno, "Failed to read %s", path);

    fclose(f);

    glShaderSource(shader, 1, (const char * const*)&source, NULL);
    free(source);
    glCompileShader(shader);

    if (checkStatus(shader) == ERR) return 0;

    return shader;
}

void debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                   GLsizei length, const GLchar *message, const void *param)
{
    error(0, 0, "%s", message);
}

int main(int argc, char *argv[])
{
    GLFWwindow *window;
    if (!glfwInit()) error(-1, 0, "Failed to init GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_DEBUG, true);
    glfwWindowHint(GLFW_RESIZABLE, false);
    window = glfwCreateWindow(800, 640, "Fonter", NULL, NULL);
    if (!window) error(-1, 0, "Failed to init window");

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        error(-1, 0, "Failed to init GLAD");

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
    glDebugMessageCallback(debugCallback, NULL);

    if (argc < 2)
    {
        argc = 2;
        argv[1] = "/usr/share/fonts/noto/NotoSerif-Regular.ttf";
    }

    uint8_t *data = NULL;
    {
        FILE *fontfile = fopen(argv[1], "rb");
        if (readall(fontfile, &data) == -1)
            error(-1, errno, "Failed to read %s", fontfile);
        fclose(fontfile);
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
    uint16_t c = utf8_codepoint("h");
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
        GLuint vs = compileShader("quad.glsl", GL_VERTEX_SHADER);
        if (vs == 0) return -1;
        GLuint fs = compileShader("sdf.glsl", GL_FRAGMENT_SHADER);
        if (fs == 0) return -1;

        shader = glCreateProgram();
        glAttachShader(shader, vs);
        glAttachShader(shader, fs);
        glLinkProgram(shader);

        glDeleteShader(vs);
        glDeleteShader(fs);

        if (checkStatus(shader) == ERR) return -1;
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
    int u_num_contours = glGetUniformLocation(shader, "num_contours");
    int u_num_points = glGetUniformLocation(shader, "num_points");

    bool has_drawn = false;
    while (!glfwWindowShouldClose(window))
    {
        if (!has_drawn)
        {
            glClear(GL_COLOR_BUFFER_BIT);

            glBindVertexArray(vao);
            glUniform1i(u_points, 0);
            glUniform1i(u_endpoints, 1);
            glUniform1ui(u_num_contours, glyph.num_contours);
            glUniform1ui(u_num_points, ttf_num_points(&glyph));
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
