#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GLuint shader(GLenum type, const char *text) {
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &text, NULL);
    glCompileShader(result);
    GLint success = 0;
    glGetShaderiv(result, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetShaderInfoLog(result, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compilation failed: %s\n", log);
        exit(1);
    }
    return result;
}
int main(int argc, char **argv) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major, minor, count;
    if (!eglInitialize(display, &major, &minor)) {
        fprintf(stderr, "eglInitialize failed %#x\n", eglGetError());
        return 1;
    }
    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_NONE
    };
    EGLConfig config;
    if (!eglChooseConfig(display, config_attributes, &config, 1, &count) || count < 1)
        return 1;
    const EGLint surface_attributes[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attributes);
    const EGLint context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    if (context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE ||
        !eglMakeCurrent(display, surface, surface, context)) return 1;
    printf("EGL=%d.%d VENDOR=%s RENDERER=%s VERSION=%s\n",
           major, minor, glGetString(GL_VENDOR), glGetString(GL_RENDERER),
           glGetString(GL_VERSION));
    GLuint vertex = shader(GL_VERTEX_SHADER,
        "attribute vec2 position;void main(){gl_Position=vec4(position,0.0,1.0);}");
    GLuint fragment = shader(GL_FRAGMENT_SHADER,
        "precision highp float;void main(){gl_FragColor=vec4(gl_FragCoord.xy/64.0,0.25,1.0);}");
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "position");
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "Program linking failed: %s\n", log);
        return 1;
    }
    const GLfloat positions[] = { -1,-1, 3,-1, -1,3 };
    glUseProgram(program);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, positions);
    glEnableVertexAttribArray(0);
    glViewport(0, 0, 64, 64);
    unsigned char pixels[64 * 64 * 4];
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int frame = 0; frame < 120; frame++) {
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        GLenum error = glGetError();
        if (error) {
            fprintf(stderr, "GL error %#x at frame %d\n", error, frame);
            return 1;
        }
        int first = (16 * 64 + 16) * 4;
        int second = (48 * 64 + 48) * 4;
        if (abs(pixels[first] - 66) > 2 || abs(pixels[first + 1] - 66) > 2 ||
            abs(pixels[second] - 193) > 2 || abs(pixels[second + 1] - 193) > 2 ||
            abs(pixels[first + 2] - 64) > 2 || pixels[first + 3] != 255) {
            fprintf(stderr, "Pixel mismatch at frame %d\n", frame);
            return 1;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("SHADERS=PASS FRAMES=120 PIXEL16=%u,%u,%u,%u PIXEL48=%u,%u,%u,%u TIME_MS=%.2f\n",
           pixels[(16*64+16)*4], pixels[(16*64+16)*4+1],
           pixels[(16*64+16)*4+2], pixels[(16*64+16)*4+3],
           pixels[(48*64+48)*4], pixels[(48*64+48)*4+1],
           pixels[(48*64+48)*4+2], pixels[(48*64+48)*4+3],
           (end.tv_sec-start.tv_sec)*1000.0+(end.tv_nsec-start.tv_nsec)/1000000.0);
    if (argc > 1) {
        FILE *file = fopen(argv[1], "wb");
        if (!file) return 1;
        fprintf(file, "P6\n64 64\n255\n");
        for (int y = 63; y >= 0; y--)
            for (int x = 0; x < 64; x++)
                fwrite(&pixels[(y*64+x)*4], 1, 3, file);
        fclose(file);
    }
    glDeleteProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    return 0;
}
