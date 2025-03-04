#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <cairo/cairo-xlib.h>

#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>

constexpr bool FILL_UP = true;  // Gradually fill up the screen with water

constexpr int    NUM_DROPS    = 500;
constexpr double DROP_LENGTH  = 8.0;
constexpr double DROP_WIDTH   = 1.25;
constexpr auto   FRAME_DELAY  = std::chrono::microseconds(1000000 / 30);
double           time_elapsed = 0.0;

struct Raindrop {
    double x, y;
    double vx, vy;

    Raindrop() = default;

    Raindrop(double width, double height, std::mt19937& gen) {
        std::uniform_real_distribution<double> dis_x(0, width);
        std::uniform_real_distribution<double> dis_y(0, height);
        std::uniform_real_distribution<double> vel(1.0, 8.0);

        x  = dis_x(gen);
        y  = dis_y(gen);
        vy = vel(gen);
        vx = 1;  // You can change that, I like the drops moving slightly towards right
    }

    void Tick(double width, double height) {
        x += vx;
        y += vy;

        double tail_length = (fabs(vx) + fabs(vy));

        if (x + tail_length * DROP_LENGTH / 2.0 < 0) {
            x = width;
        } else if (x - tail_length * DROP_LENGTH / 2.0 > width) {
            x = 0;
        }

        if constexpr (FILL_UP) {
            double y_offset = height - (time_elapsed * 0.5);
            double wave_y   = y_offset + 5.0 * sin((x * 0.05) + time_elapsed) * cos((x * -0.025) + time_elapsed);

            if (y > wave_y) {
                y = 0;
            }
        } else if (y - tail_length * DROP_LENGTH / 2.0 > height) {
            y = 0;
        }
    }

    void Draw(cairo_t* cr) {
        double theta = atan2(vy, vx) - M_PI / 2.0;  // Direction vector

        double base_radius = DROP_WIDTH * 2.0;
        double tri_height  = (fabs(vx) + fabs(vy)) * DROP_LENGTH / 2.0;

        cairo_save(cr);

        cairo_translate(cr, x, y);
        cairo_rotate(cr, theta);  // Rotate coordinates system so the Raindrop aligns with velocity vector

        /* Make the drop */
        cairo_arc(cr, 0, 0, base_radius, 0, M_PI);
        cairo_line_to(cr, -base_radius, 0);
        cairo_line_to(cr, 0, -tri_height);
        cairo_line_to(cr, base_radius, 0);
        cairo_close_path(cr);

        cairo_fill(cr);

        cairo_restore(cr);
    }
};

int main() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::cerr << "Cannot open display" << std::endl;
        return 1;
    }

    int    screen = DefaultScreen(display);
    Window root   = RootWindow(display, screen);
    int    width  = DisplayWidth(display, screen);
    int    height = DisplayHeight(display, screen);

    std::cout << "Screen info: Width=" << width << ", Height=" << height << ", Default Depth=" << DefaultDepth(display, screen) << std::endl;

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo)) {
        std::cerr << "No 32-bit TrueColor visual found. Transparency may not work." << std::endl;
        vinfo.visual = DefaultVisual(display, screen);
        vinfo.depth  = DefaultDepth(display, screen);
    }
    Visual* visual = vinfo.visual;
    int     depth  = vinfo.depth;
    std::cout << "Using visual ID=0x" << std::hex << vinfo.visualid << std::dec << ", Depth=" << depth << std::endl;

    Colormap colormap = XCreateColormap(display, root, visual, AllocNone);

    XSetWindowAttributes attrs = {};
    attrs.override_redirect    = true;  // Bypass window manager
    attrs.border_pixel         = 0;
    attrs.colormap             = colormap;
    attrs.background_pixmap    = None;  // Transparency
    attrs.event_mask           = None;  // Don't capture any events
    unsigned long attr_mask    = CWOverrideRedirect | CWBorderPixel | CWColormap | CWBackPixmap | CWEventMask;

    Window win = XCreateWindow(display, root, 0, 0, width, height, 0, depth, InputOutput, visual, attr_mask, &attrs);
    if (!win) {
        std::cerr << "XCreateWindow failed" << std::endl;
        XFreeColormap(display, colormap);
        XCloseDisplay(display);
        return 1;
    }

    /* Set window type as an overlay (removes blur effects and such from compositors (picom at least :P)) */
    Atom window_type  = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom overlay_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(display, win, window_type, XA_ATOM, 32, PropModeReplace, (unsigned char*)&overlay_type, 1);

    /* Keep the window on top */
    Atom wm_state    = XInternAtom(display, "_NET_WM_STATE", False);
    Atom state_above = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(display, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)&state_above, 1);

    XMapWindow(display, win);

    /* Ensure it’s raised */
    XRaiseWindow(display, win);

    /* Make the window click-through for mouse events by creating an empty region and using it as the input shape */
    int fixes_event_base, fixes_error_base;
    if (XFixesQueryExtension(display, &fixes_event_base, &fixes_error_base)) {
        XRectangle    rects[1] = {{0, 0, 0, 0}};
        XserverRegion region   = XFixesCreateRegion(display, rects, 0);
        XFixesSetWindowShapeRegion(display, win, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(display, region);
        std::cout << "XFixes extension enabled: event_base=" << fixes_event_base << ", error_base=" << fixes_error_base << ", window set to click-through for mouse" << std::endl;
    } else {
        std::cout << "XFixes extension not available, window will not be click-through" << std::endl;
    }

    cairo_surface_t* surface = cairo_xlib_surface_create(display, win, visual, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "Cairo surface error: " << cairo_status_to_string(cairo_surface_status(surface)) << std::endl;
        XDestroyWindow(display, win);
        XFreeColormap(display, colormap);
        XCloseDisplay(display);
        return 1;
    }
    cairo_t* cr = cairo_create(surface);

    std::random_device              rd;
    std::mt19937                    gen(rd());
    std::array<Raindrop, NUM_DROPS> drops;

    for (auto& drop : drops) {
        drop = Raindrop(width, height, gen);
    }

    std::cout << "Setup complete, starting animation..." << std::endl;

    bool running = true;
    while (running) {
        time_elapsed += 0.1;

        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0, 0, 0, 0);  // Transparent background (clear surface)
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

        cairo_set_source_rgba(cr, 0.4, 0.8, 1.0, 0.5);  // Rain color

        for (auto& drop : drops) {
            drop.Tick(width, height);
            drop.Draw(cr);
        }

        /* Fill the screen the water over time */
        if constexpr (FILL_UP) {
            double y_offset = height - (time_elapsed * NUM_DROPS * 0.001);
            cairo_move_to(cr, 0, y_offset);
            for (int x = 0; x < width; x += 10) {
                double y = y_offset + 5.0 * sin((x * 0.05) + time_elapsed) * cos((x * -0.025) + time_elapsed);
                if (y <= 0) {
                    time_elapsed = 0;
                }
                cairo_line_to(cr, x, y);
            }
            cairo_line_to(cr, width, height);
            cairo_line_to(cr, 0, height);
            cairo_close_path(cr);
            cairo_fill(cr);
        }

        cairo_surface_flush(surface);
        XFlush(display);
        std::this_thread::sleep_for(FRAME_DELAY);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    XDestroyWindow(display, win);
    XFreeColormap(display, colormap);
    XCloseDisplay(display);

    return 0;
}
