#include "gl_render_layer.hpp"
#include "error.hpp"
#include "iris/io/io.hpp"
#include "iris/runtime.hpp"
#include "state.hpp"
#include <iris/graphics/windowing.hpp>
#include <GLFW/glfw3.h>

namespace {
    static u32 glfw_inits = 0;

    namespace callback {
        void glfw_error(int code, const char *description) {
            iris::error(iris::error_code::native_error, std::format("GLFW error {}: {}", code, description));
        }
    }

    void glfw_init() {
        if (glfw_inits == 0) {
            if (i32 ret = glfwInit(); ret != GLFW_TRUE) {
                iris::error(iris::error_code::native_error, "glfwInit() failed");
            }
            glfwSetErrorCallback(callback::glfw_error);
            ++glfw_inits;
        }
    }

    void glfw_terminate() {
        if (glfw_inits == 0) {
            glfwTerminate();
        } else {
            --glfw_inits;
        }        
    }

    struct gpu_capabilities {
    public:
        u32 max_msaa_samples;
        u32 max_gl_version;
    public:
        static gpu_capabilities query() noexcept {
            gpu_capabilities cap;
            glfw_init();
            static u32 query_versions[] = {
                Iris_EncodeGLVersion(4, 6),
                Iris_EncodeGLVersion(4, 1),
                Iris_EncodeGLVersion(3, 3)
            };
            for (u32 ver : query_versions) {
                u32 major = Iris_DecodeGLVersionMajor(ver);
                u32 minor = Iris_DecodeGLVersionMinor(ver);

                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
                GLFWwindow *win = glfwCreateWindow(1, 1, "Iris — GPU Capabilities Query Window", nullptr, nullptr);
                auto cleanup = [&]() -> void {
                    glfwMakeContextCurrent(nullptr);
                    if (win != nullptr)
                        glfwDestroyWindow(win);
                };
                if (win == nullptr) {
                    cleanup();
                    continue;
                } 
                glfwMakeContextCurrent(win);
                i32 max_msaa = 0;
                glGetIntegerv(GL_MAX_SAMPLES, &max_msaa);
                i32 actual_major, actual_minor;
                glGetIntegerv(GL_MAJOR_VERSION, &actual_major);
                glGetIntegerv(GL_MINOR_VERSION, &actual_minor);
                if (actual_major != i32(major) || actual_minor != i32(minor)) {
                    cleanup();
                    continue;
                }
                cap.max_gl_version = Iris_EncodeGLVersion(major, minor);
                cap.max_msaa_samples = max_msaa;
                cleanup();
                break;
            }
            glfw_terminate();
            return cap;
        }
    private:
        gpu_capabilities() = default;
    };
}

namespace iris {
    void window::create(const std::string &title, glm::ivec2 size) noexcept {
        glfw_init();
        gpu_capabilities gpu_cap = gpu_capabilities::query();
        if (gpu_cap.max_gl_version >= 43) {
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        }
        if (this->config.msaa_samples > 0) {
            if (!(this->config.msaa_samples == 2 || this->config.msaa_samples == 4 || this->config.msaa_samples == 8)) {
                iris::error(error_code::invalid_configuration, std::format("MSAA Samples value {} is invalid, valid values are {}, {}, {}", static_cast<u32>(this->config.msaa_samples), 2, 4, 8));
                std::exit(1);
            }
            if (gpu_cap.max_msaa_samples > 0) {
                glfwWindowHint(GLFW_SAMPLES, std::min(this->config.msaa_samples, gpu_cap.max_msaa_samples));
            } else {
                iris::error(error_code::incapable_hardware, "Hardware does not support requested MSAS sample value");
                std::exit(1);
            }
        } 
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, Iris_DecodeGLVersionMajor(gpu_cap.max_gl_version));
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, Iris_DecodeGLVersionMinor(gpu_cap.max_gl_version));
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        this->handle = glfwCreateWindow(
            size.x,
            size.y,
            title.c_str(),
            nullptr,
            nullptr
        );

        if (this->handle == nullptr) {
            iris::error(iris::error_code::native_error, "Window creation failed: glfwCreateWindow returned nullptr");
            std::exit(1);
        }

        internal::g_state.gl_state.gl_version = gpu_cap.max_gl_version;

        glfwSetWindowUserPointer(reinterpret_cast<GLFWwindow*>(this->handle), this);

        // whatever i'll keep this?
        glfwShowWindow(reinterpret_cast<GLFWwindow*>(this->handle));

        GLFWwindow *cur_ctx = glfwGetCurrentContext();
        glfwMakeContextCurrent(reinterpret_cast<GLFWwindow*>(this->handle));

        if (this->config.vsync) {
            glfwSwapInterval(1);
        } else {
            glfwSwapInterval(0);
        }

        glfwMakeContextCurrent(cur_ctx);
    }

    glm::ivec2 window::framebuffer_size() const noexcept {
        int w, h;
        glfwGetFramebufferSize(reinterpret_cast<GLFWwindow*>(this->handle), &w, &h);
        return { w, h };
    }

    bool window::running() const noexcept {
        return !glfwWindowShouldClose(reinterpret_cast<GLFWwindow*>(this->handle));
    }

    bool window::visible_surface() const noexcept {
        return glfwGetWindowAttrib(reinterpret_cast<GLFWwindow*>(this->handle), GLFW_VISIBLE);
    }

    void window::swap_buffers() const noexcept {
        glfwSwapBuffers(reinterpret_cast<GLFWwindow*>(this->handle));
    }

    std::unordered_map<i32, glm::vec2> window::touch_points() noexcept {
        return {};
    }

    void window::make_gl_context_current() const noexcept {
        glfwMakeContextCurrent(reinterpret_cast<GLFWwindow*>(this->handle));
    }

    static io::key glfw_to_iris(i32 k) noexcept {
        switch (k) {
            case GLFW_KEY_A: return io::key::a;
            case GLFW_KEY_B: return io::key::b;
            case GLFW_KEY_C: return io::key::c;
            case GLFW_KEY_D: return io::key::d;
            case GLFW_KEY_E: return io::key::e;
            case GLFW_KEY_F: return io::key::f;
            case GLFW_KEY_G: return io::key::g;
            case GLFW_KEY_H: return io::key::h;
            case GLFW_KEY_I: return io::key::i;
            case GLFW_KEY_J: return io::key::j;
            case GLFW_KEY_K: return io::key::k;
            case GLFW_KEY_L: return io::key::l;
            case GLFW_KEY_M: return io::key::m;
            case GLFW_KEY_N: return io::key::n;
            case GLFW_KEY_O: return io::key::o;
            case GLFW_KEY_P: return io::key::p;
            case GLFW_KEY_Q: return io::key::q;
            case GLFW_KEY_R: return io::key::r;
            case GLFW_KEY_S: return io::key::s;
            case GLFW_KEY_T: return io::key::t;
            case GLFW_KEY_U: return io::key::u;
            case GLFW_KEY_V: return io::key::v;
            case GLFW_KEY_W: return io::key::w;
            case GLFW_KEY_X: return io::key::x;
            case GLFW_KEY_Y: return io::key::y;
            case GLFW_KEY_Z: return io::key::z;

            case GLFW_KEY_0: return io::key::num_0;
            case GLFW_KEY_1: return io::key::num_1;
            case GLFW_KEY_2: return io::key::num_2;
            case GLFW_KEY_3: return io::key::num_3;
            case GLFW_KEY_4: return io::key::num_4;
            case GLFW_KEY_5: return io::key::num_5;
            case GLFW_KEY_6: return io::key::num_6;
            case GLFW_KEY_7: return io::key::num_7;
            case GLFW_KEY_8: return io::key::num_8;
            case GLFW_KEY_9: return io::key::num_9;

            case GLFW_KEY_SPACE: return io::key::space;
            case GLFW_KEY_APOSTROPHE: return io::key::apostrophe;
            case GLFW_KEY_COMMA: return io::key::comma;
            case GLFW_KEY_MINUS: return io::key::minus;
            case GLFW_KEY_PERIOD: return io::key::period;
            case GLFW_KEY_SLASH: return io::key::slash;
            case GLFW_KEY_SEMICOLON: return io::key::semicolon;
            case GLFW_KEY_EQUAL: return io::key::equal;
            case GLFW_KEY_LEFT_BRACKET: return io::key::left_bracket;
            case GLFW_KEY_BACKSLASH: return io::key::backslash;
            case GLFW_KEY_RIGHT_BRACKET: return io::key::right_bracket;
            case GLFW_KEY_GRAVE_ACCENT: return io::key::grave;

            case GLFW_KEY_ESCAPE: return io::key::escape;
            case GLFW_KEY_ENTER: return io::key::enter;
            case GLFW_KEY_TAB: return io::key::tab;
            case GLFW_KEY_BACKSPACE: return io::key::backspace;
            case GLFW_KEY_INSERT: return io::key::insert;
            case GLFW_KEY_DELETE: return io::key::delete_;

            case GLFW_KEY_RIGHT: return io::key::right;
            case GLFW_KEY_LEFT: return io::key::left;
            case GLFW_KEY_DOWN: return io::key::down;
            case GLFW_KEY_UP: return io::key::up;

            case GLFW_KEY_PAGE_UP: return io::key::page_up;
            case GLFW_KEY_PAGE_DOWN: return io::key::page_down;
            case GLFW_KEY_HOME: return io::key::home;
            case GLFW_KEY_END: return io::key::end;

            case GLFW_KEY_CAPS_LOCK: return io::key::caps_lock;
            case GLFW_KEY_SCROLL_LOCK: return io::key::scroll_lock;
            case GLFW_KEY_NUM_LOCK: return io::key::num_lock;
            case GLFW_KEY_PRINT_SCREEN: return io::key::print_screen;
            case GLFW_KEY_PAUSE: return io::key::pause;

            case GLFW_KEY_F1: return io::key::f1;
            case GLFW_KEY_F2: return io::key::f2;
            case GLFW_KEY_F3: return io::key::f3;
            case GLFW_KEY_F4: return io::key::f4;
            case GLFW_KEY_F5: return io::key::f5;
            case GLFW_KEY_F6: return io::key::f6;
            case GLFW_KEY_F7: return io::key::f7;
            case GLFW_KEY_F8: return io::key::f8;
            case GLFW_KEY_F9: return io::key::f9;
            case GLFW_KEY_F10: return io::key::f10;
            case GLFW_KEY_F11: return io::key::f11;
            case GLFW_KEY_F12: return io::key::f12;
            case GLFW_KEY_F13: return io::key::f13;
            case GLFW_KEY_F14: return io::key::f14;
            case GLFW_KEY_F15: return io::key::f15;
            case GLFW_KEY_F16: return io::key::f16;
            case GLFW_KEY_F17: return io::key::f17;
            case GLFW_KEY_F18: return io::key::f18;
            case GLFW_KEY_F19: return io::key::f19;
            case GLFW_KEY_F20: return io::key::f20;
            case GLFW_KEY_F21: return io::key::f21;
            case GLFW_KEY_F22: return io::key::f22;
            case GLFW_KEY_F23: return io::key::f23;
            case GLFW_KEY_F24: return io::key::f24;

            case GLFW_KEY_KP_0: return io::key::keypad_0;
            case GLFW_KEY_KP_1: return io::key::keypad_1;
            case GLFW_KEY_KP_2: return io::key::keypad_2;
            case GLFW_KEY_KP_3: return io::key::keypad_3;
            case GLFW_KEY_KP_4: return io::key::keypad_4;
            case GLFW_KEY_KP_5: return io::key::keypad_5;
            case GLFW_KEY_KP_6: return io::key::keypad_6;
            case GLFW_KEY_KP_7: return io::key::keypad_7;
            case GLFW_KEY_KP_8: return io::key::keypad_8;
            case GLFW_KEY_KP_9: return io::key::keypad_9;
            case GLFW_KEY_KP_DECIMAL: return io::key::keypad_decimal;
            case GLFW_KEY_KP_DIVIDE: return io::key::keypad_divide;
            case GLFW_KEY_KP_MULTIPLY: return io::key::keypad_multiply;
            case GLFW_KEY_KP_SUBTRACT: return io::key::keypad_subtract;
            case GLFW_KEY_KP_ADD: return io::key::keypad_add;
            case GLFW_KEY_KP_ENTER: return io::key::keypad_enter;
            case GLFW_KEY_KP_EQUAL: return io::key::keypad_equal;

            case GLFW_KEY_LEFT_SHIFT: return io::key::left_shift;
            case GLFW_KEY_LEFT_CONTROL: return io::key::left_control;
            case GLFW_KEY_LEFT_ALT: return io::key::left_alt;
            case GLFW_KEY_LEFT_SUPER: return io::key::left_super;

            case GLFW_KEY_RIGHT_SHIFT: return io::key::right_shift;
            case GLFW_KEY_RIGHT_CONTROL: return io::key::right_control;
            case GLFW_KEY_RIGHT_ALT: return io::key::right_alt;
            case GLFW_KEY_RIGHT_SUPER: return io::key::right_super;

            case GLFW_KEY_MENU: return io::key::menu;
        }

        return io::key::none;
    }

    void window::poll_events() noexcept {
        glfwPollEvents();
        for (int i = GLFW_KEY_SPACE; i <= GLFW_KEY_LAST; ++i) {
            const io::key key = glfw_to_iris(i);
            i32 state = glfwGetKey(reinterpret_cast<GLFWwindow*>(this->handle), i);
            size_t idx = static_cast<u32>(key);
            if (state == GLFW_PRESS) {
                this->keys[idx].make_down();
            } else if (state == GLFW_RELEASE) {
                this->keys[idx].make_released();
            }
        }
    }

    io::key_state window::key_state(io::key key) const noexcept {
        return this->keys[static_cast<u32>(key)];
    }

    window::window() noexcept {
        this->create("Iris — Unnamed Window", { 800, 600 });
    }

    window::window(const std::string &title, glm::ivec2 sz, const struct config &config) noexcept {
        this->config = config;
        this->create(title, sz);
    }

    window::~window() {
        glfwMakeContextCurrent(nullptr);
        glfwDestroyWindow(reinterpret_cast<GLFWwindow*>(this->handle));
        glfw_terminate();
    }
}
