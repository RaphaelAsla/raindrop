/* clang-format off */
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <GL/glx.h>
#include <X11/Xutil.h>
#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
#include <iostream>
#include <vector>
#include <random>

#include "../includes/shader.hpp"
/* clang-format on */

constexpr int NUM_DROPS = 500;

struct Raindrop {
    float x, y, vx, vy;
};

void applyX11Tweaks(Display* display, Window xwindow) {
    XUnmapWindow(display, xwindow);
    XFlush(display);

    XSetWindowAttributes attrs = {};
    attrs.override_redirect    = true;
    unsigned long attr_mask    = CWOverrideRedirect;

    XChangeWindowAttributes(display, xwindow, attr_mask, &attrs);

    Atom windowTypeAtom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom overlayAtom    = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(display, xwindow, windowTypeAtom, XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&overlayAtom), 1);

    Atom wmStateAtom    = XInternAtom(display, "_NET_WM_STATE", False);
    Atom stateAboveAtom = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(display, xwindow, wmStateAtom, XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&stateAboveAtom), 1);

    int fixesEventBase, fixesErrorBase;
    if (XFixesQueryExtension(display, &fixesEventBase, &fixesErrorBase)) {
        XRectangle    rect   = {0, 0, 0, 0};
        XserverRegion region = XFixesCreateRegion(display, &rect, 1);
        XFixesSetWindowShapeRegion(display, xwindow, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(display, region);
        std::cout << "Window set to click-through (using XFixes)" << std::endl;
    } else {
        std::cout << "XFixes extension not available; window will not be click-through" << std::endl;
    }

    XMapWindow(display, xwindow);
    XRaiseWindow(display, xwindow);
    XFlush(display);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    GLFWmonitor*       primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode           = glfwGetVideoMode(primaryMonitor);

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Overlay", primaryMonitor, nullptr);
    glfwSetWindowPos(window, 0, 0);

    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    Display* display = glfwGetX11Display();
    Window   xwindow = glfwGetX11Window(window);
    applyX11Tweaks(display, xwindow);

    glfwMakeContextCurrent(window);

    // Initialize GLAD
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Create and compile shaders
    Shader shader("../shaders/vert.glsl", "../shaders/frag.glsl");

    // Create a simple quad to render
    float vertices[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Initialize raindrops
    std::vector<Raindrop>                 raindrops(NUM_DROPS);
    std::random_device                    rd;
    std::mt19937                          gen(rd());
    std::uniform_real_distribution<float> dist_x(0.0f, 1.0f);
    std::uniform_real_distribution<float> dist_y(0.0f, 1.0f);
    std::uniform_real_distribution<float> vel(0.5f, 1.0f);

    for (auto& drop : raindrops) {
        drop.x  = dist_x(gen);  // Random x position
        drop.y  = dist_y(gen);  // Random y position
        drop.vx = 0.1f;         // Not used in SSBO for now
        drop.vy = vel(gen);     // Default velocity, adjust in shader
    }

    // Create and populate SSBO
    unsigned int drops_ssbo;
    glGenBuffers(1, &drops_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, drops_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_DROPS * sizeof(Raindrop), raindrops.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, drops_ssbo);  // Binding point 0
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);                  // Unbind

    // Use shader and set numDrops
    shader.use();
    shader.setInt("numDrops", NUM_DROPS);
    shader.setVec2("resolution", mode->width, mode->height);

    // Main render loop
    float time = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        // Use shader and update time
        shader.use();
        time += 0.016f;  // Approximately 60 FPS increment
        shader.setFloat("time", time);

        // Draw quad
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
