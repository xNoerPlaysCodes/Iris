#include "android_native_app_glue.h"
#include "error.hpp"
#include "glm/fwd.hpp"
#include "iris/graphics/windowing.hpp"
#include "iris/io/io.hpp"
#include "iris/runtime.hpp"
#include "nutils/types.hpp"
#include <android/native_window.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <array>
#include <gl.h>
#include <string>

extern android_app *g_android_app;

constexpr std::array<i32, 13> attribs = {
    EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT,
    EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
    EGL_RED_SIZE,
        8,
    EGL_GREEN_SIZE,
        8,
    EGL_BLUE_SIZE,
        8,
    EGL_ALPHA_SIZE,
        8,
    EGL_NONE
};

namespace {
    struct anw_handle {
        std::unordered_map<i32, glm::vec2> touch_points;
        ANativeWindow *window;
        EGLConfig config;
        EGLDisplay display;
        EGLContext context;
        EGLSurface surface;
        bool running = true;
    };
}

namespace iris {
    void window::create(const std::string &, glm::ivec2) noexcept {
        this->config.gles = true;
        this->handle = new anw_handle();
        ANativeWindow *window = g_android_app->window;
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (!eglInitialize(display, nullptr, nullptr)) {
            iris::error(error_code::native_error, "Window creation failed: eglInitialize() returned nullptr");
            std::exit(1);
        }


        EGLConfig config;
        EGLint numConfigs;
        eglChooseConfig(display, attribs.data(), &config, 1, &numConfigs);

        EGLint context_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };

        EGLContext context = eglCreateContext(display, config, nullptr, context_attribs);

        const char* egl_extensions = eglQueryString(display, EGL_EXTENSIONS);
        bool has_colorspace = egl_extensions != nullptr && strstr(egl_extensions, "EGL_KHR_gl_colorspace");

        EGLSurface surface;

        if (has_colorspace) {
            EGLint srgb_attribs[] = {
                // EGL_GL_COLORSPACE_SRGB_KHR, EGL_GL_COLORSPACE_SRGB_KHR,
                // FIXME: srgb just doens't work on non-emulators???
                EGL_NONE
            };
            surface = eglCreateWindowSurface(display, config, window, srgb_attribs);
        } else {
            surface = eglCreateWindowSurface(display, config, window, nullptr); 
        }

        i32 err = eglGetError();
        if (err != EGL_SUCCESS) {
            iris::error(error_code::native_error, std::format("eglGetError() = {}", err));
            std::exit(1);
        }

        if (!eglMakeCurrent(display, surface, surface, context)) {
            iris::error(error_code::native_error, "eglMakeCurrent() failed");
            std::exit(1);
        }

        if (this->config.vsync) {
            eglSwapInterval(display, 1);
        } else {
            eglSwapInterval(display, 0);
        }

        g_android_app->userData = this;

        anw_handle *hdl = reinterpret_cast<anw_handle*>(this->handle);
        hdl->display = display;
        hdl->config = config;
        hdl->window = window;
        hdl->context = context;
        hdl->surface = surface;

        g_android_app->onInputEvent = [](android_app *app, AInputEvent *event) -> i32 {
            iris::window *win = reinterpret_cast<iris::window*>(app->userData);
            anw_handle *hdl = reinterpret_cast<anw_handle*>(win->handle);
            i32 type = AInputEvent_getType(event);

            if (type == AINPUT_EVENT_TYPE_MOTION) {
                i32 action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
                float x = AMotionEvent_getX(event, 0);
                float y = AMotionEvent_getY(event, 0);
                i32 id = AMotionEvent_getPointerId(event, 0);

                if (action == AMOTION_EVENT_ACTION_DOWN) {
                    hdl->touch_points[id] = { x, y };
                } else if (action == AMOTION_EVENT_ACTION_UP) {
                    auto it = hdl->touch_points.find(id);
                    if (it != hdl->touch_points.end()) {
                        hdl->touch_points.erase(it);
                    }
                } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                    auto it = hdl->touch_points.find(id);
                    if (it != hdl->touch_points.end()) {
                        it->second = { x, y };
                    }
                }

                return 1;
            }
            return 0;
        };

        g_android_app->onAppCmd = [](android_app *app, i32 cmd) -> void {
            iris::window *win = reinterpret_cast<iris::window*>(app->userData);
            anw_handle *hdl = reinterpret_cast<anw_handle*>(win->handle);
            if (cmd == APP_CMD_TERM_WINDOW) {
                eglMakeCurrent(hdl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, hdl->context);
                eglDestroySurface(hdl->display, hdl->surface);
                hdl->surface = nullptr;
                hdl->window = nullptr;
            } else if (cmd == APP_CMD_INIT_WINDOW) {
                hdl->window = app->window;
                const char* egl_extensions = eglQueryString(hdl->display, EGL_EXTENSIONS);
                bool has_colorspace = egl_extensions != nullptr && strstr(egl_extensions, "EGL_KHR_gl_colorspace");

                EGLSurface surface;

                if (has_colorspace) {
                    EGLint srgb_attribs[] = {
                        EGL_GL_COLORSPACE_SRGB_KHR, EGL_GL_COLORSPACE_SRGB_KHR,
                        EGL_NONE
                    };
                    surface = eglCreateWindowSurface(hdl->display, hdl->config, hdl->window, srgb_attribs);
                } else {
                    surface = eglCreateWindowSurface(hdl->display, hdl->config, hdl->window, nullptr); 
                }

                hdl->surface = surface;

                eglMakeCurrent(hdl->display, surface, surface, hdl->context);
            }
        };

        this->make_gl_context_current();
    }

    void window::make_gl_context_current() const noexcept {
        anw_handle *hdl = reinterpret_cast<anw_handle*>(this->handle);
        eglMakeCurrent(hdl->display, hdl->surface, hdl->surface, hdl->context);
    }

    window::window() noexcept {
        this->create("Iris — Unnamed Window", { 800, 600 });
    }

    window::window(const std::string &title, glm::ivec2 size, const struct config &config) noexcept {
        this->create(title, size);
    }

    io::key_state window::key_state(io::key key) const noexcept {
        return this->keys[static_cast<u32>(key)];
    }

    std::unordered_map<i32, glm::vec2> window::touch_points() noexcept {
        return reinterpret_cast<anw_handle*>(this->handle)->touch_points;
    }

    void window::swap_buffers() const noexcept {
        anw_handle *hdl = reinterpret_cast<anw_handle*>(this->handle);
        eglSwapBuffers(hdl->display, hdl->surface);
    }

    bool window::running() const noexcept {
        return reinterpret_cast<anw_handle*>(this->handle)->running;
    }

    bool window::visible_surface() const noexcept {
        anw_handle *hdl = reinterpret_cast<anw_handle*>(this->handle);
        i32 width, height;
        eglQuerySurface(hdl->display, hdl->surface, EGL_WIDTH, &width);

        i32 err = eglGetError();
        if (err == EGL_BAD_SURFACE) {
            return false;
        }

        eglQuerySurface(hdl->display, hdl->surface, EGL_HEIGHT, &height);

        return width > 0 && height > 0;
    }

    void window::poll_events() noexcept {
        anw_handle *hdl = reinterpret_cast<anw_handle*>(this->handle);

        if (g_android_app != nullptr) {
            int events;
            android_poll_source* source;
            while (ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
                if (source) source->process(g_android_app, source);
                if (g_android_app->destroyRequested) {
                    hdl->running = false;
                    return;
                }
            }
        }

        if (hdl->touch_points.size() > 0) {
            this->keys[static_cast<u32>(io::key::finger)].make_down();
        } else {
            this->keys[static_cast<u32>(io::key::finger)].make_released();
        }
    }

    glm::ivec2 window::framebuffer_size() const noexcept {
        anw_handle *hdl = reinterpret_cast<anw_handle*>(this->handle);
        i32 width, height;
        eglQuerySurface(hdl->display, hdl->surface, EGL_WIDTH, &width);
        eglQuerySurface(hdl->display, hdl->surface, EGL_HEIGHT, &height);

        return { width, height };
    }

    window::~window() {
        delete reinterpret_cast<anw_handle*>(this->handle);
    }
}

// set this->config.gles = true PLEASE
