/* clang-format off */
#include "glad/gl.h"
#include <GL/glx.h>
#include <X11/Xutil.h>
#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
/* clang-format on */

constexpr int NUM_DROPS    = 500;
double        time_elapsed = 0.0;
const auto    FRAME_DELAY  = std::chrono::microseconds(1000000 / 30);

struct Raindrop {
    float x, y, vx, vy;
};
struct Ball {
    float x, y, rotation, padding;
};

// Function to check shader compilation errors
void checkShaderCompilation(GLuint shader, const char* type) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::" << type << "::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
}

// Function to check program linking errors
void checkProgramLinking(GLuint program) {
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
}

// Compile displayProgram
GLuint compileDisplayProgram() {
    // Shader source code
    const char* vertSrc = R"(
        #version 450
        layout(location = 0) in vec2 pos;
        out vec2 texCoord;
        void main() {
            gl_Position = vec4(pos, 0, 1);
            texCoord = (pos + 1) / 2;
        }
    )";
    const char* fragSrc = R"(
        #version 450
        in vec2 texCoord;
        out vec4 fragColor;
        uniform sampler2D tex;
        void main() {
            fragColor = texture(tex, texCoord);
        }
    )";

    // Create shader objects
    GLuint vertexShader   = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    // Set source code
    glShaderSource(vertexShader, 1, &vertSrc, nullptr);
    glShaderSource(fragmentShader, 1, &fragSrc, nullptr);

    // Compile shaders
    glCompileShader(vertexShader);
    checkShaderCompilation(vertexShader, "VERTEX");

    glCompileShader(fragmentShader);
    checkShaderCompilation(fragmentShader, "FRAGMENT");

    // Create and link program
    GLuint displayProgram = glCreateProgram();
    glAttachShader(displayProgram, vertexShader);
    glAttachShader(displayProgram, fragmentShader);
    glLinkProgram(displayProgram);
    checkProgramLinking(displayProgram);

    // Clean up shader objects
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return displayProgram;
}

int main() {
    // X11 window setup (unchanged until GLX context)
    Display*    display = XOpenDisplay(nullptr);
    int         screen  = DefaultScreen(display);
    Window      root    = RootWindow(display, screen);
    int         width = 1920, height = 1080;
    XVisualInfo vinfo;
    XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo);
    Colormap             colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
    XSetWindowAttributes attrs    = {.override_redirect = True, .colormap = colormap};
    Window               win      = XCreateWindow(display, root, 0, 0, width, height, 0, vinfo.depth, InputOutput, vinfo.visual, CWOverrideRedirect | CWColormap, &attrs);
    XMapWindow(display, win);

    // GLFW and GLX setup
    glfwInit();
    GLXContext glc = glXCreateContext(display, &vinfo, nullptr, True);
    glXMakeCurrent(display, win, glc);

    // Setup texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // Setup SSBOs
    std::vector<Raindrop> drops(NUM_DROPS);
    for (auto& d : drops) {
        d.x  = rand() % width;
        d.y  = rand() % height;
        d.vy = 2.0 + (rand() % 60) / 10.0;
        d.vx = 1.0;
    }
    Ball ball = {1920.0f, 0.0f, 0.0f, 0.0f};

    GLuint ssboDrops, ssboBall;
    glGenBuffers(1, &ssboDrops);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboDrops);
    glBufferData(GL_SHADER_STORAGE_BUFFER, drops.size() * sizeof(Raindrop), drops.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboDrops);

    glGenBuffers(1, &ssboBall);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboBall);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Ball), &ball, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboBall);

    // Compile compute shader (assume source in 'compute.glsl')
    GLuint      program = glCreateProgram();
    GLuint      shader  = glCreateShader(GL_COMPUTE_SHADER);
    const char* source  = "../shaders/compute.glsl";
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    glAttachShader(program, shader);
    glLinkProgram(program);
    glUseProgram(program);

    // Uniforms
    glUniform1f(glGetUniformLocation(program, "width"), width);
    glUniform1f(glGetUniformLocation(program, "height"), height);

    // Full-screen quad
    GLuint vao, vbo;
    float  quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    GLuint displayProgram = compileDisplayProgram();

    // Main loop
    while (true) {
        time_elapsed += 0.1;
        glClear(GL_COLOR_BUFFER_BIT);

        // Run compute shader
        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "timeElapsed"), time_elapsed);
        glDispatchCompute(502, 1, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Display
        glUseProgram(displayProgram);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glXSwapBuffers(display, win);
        glfwPollEvents();
        std::this_thread::sleep_for(FRAME_DELAY);
    }

    // Cleanup (omitted for brevity)
    return 0;
}
