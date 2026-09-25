#include "gl_render_layer.hpp"
#include "error.hpp"
#include "spdlog/spdlog.h"
#include "state.hpp"
#include "bitutil.hpp"

#include <gl.h>

#include <quad_frag.glsl.h>
#include <quad_vert.glsl.h>
#include <glm/gtc/type_ptr.hpp>

using namespace iris::internal;

namespace iris::gl {
    std::string replace_all(std::string str, const std::string &from, const std::string &to) {
        if (from.empty())
            return str;

        std::size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }

        return str;
    }
    u32 compile_debug_shader(std::string shader_version_string) noexcept {
        std::string vertex = quad_vert;
        vertex = replace_all(vertex, "//iris_replace_glsl_version", shader_version_string);
        std::string fragment = quad_frag;
        fragment = replace_all(fragment, "//iris_replace_glsl_version", shader_version_string);

        const char *vcs = vertex.c_str();
        const char *fcs = fragment.c_str();
        
        u32 vert, frag;
        char log[512] = {};

        vert = glCreateShader(GL_VERTEX_SHADER);
        frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(vert, 1, &vcs, nullptr);
        glShaderSource(frag, 1, &fcs, nullptr);

        glCompileShader(vert);
        glGetShaderInfoLog(vert, 512, nullptr, log);
        if (log[0] != 0) {
            spdlog::debug("logV: {}", log);
        }
        glCompileShader(frag);
        memset(log, 0, 512);
        glGetShaderInfoLog(frag, 512, nullptr, log);
        if (log[0] != 0) {
            spdlog::debug("logF: {}", log);
        }

        u32 prog = glCreateProgram();
        glAttachShader(prog, vert);
        glAttachShader(prog, frag);

        glLinkProgram(prog);

        glDeleteShader(vert);
        glDeleteShader(frag);

        return prog;
    }


    void init(const init_config &cfg) noexcept {
        gl::gl_init(); // glewInit() basically
        glViewport(
            static_cast<i32>(cfg.viewport_offset.x),
            static_cast<i32>(cfg.viewport_offset.y),
            static_cast<i32>(cfg.viewport_size.x),
            static_cast<i32>(cfg.viewport_size.y)
        );

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        i32 units = g_state.gl_state.max_texture_units;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &units);
        g_state.gl_state.max_texture_units = std::min(units, 32);

        for (i32 i = 0; i < g_state.gl_state.max_texture_units; ++i) {
            bitutil::setr(i, g_state.gl_state.texture_unit_freelist);
        }

        spdlog::info("OpenGL initialized on \"{}\"", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        spdlog::debug("GL_MAX_TEXTURE_IMAGE_UNITS: {} (hardware {})", g_state.gl_state.max_texture_units, units);
    }

    void check_error(i32 n) noexcept {
        i32 error = glGetError();
        while (error != GL_NO_ERROR) {
            iris::error(error_code::gl_error, std::format("glGetError() = {} {}", error, n == -1 ? 0 : n));
            error = glGetError();
        }
    }


    shader::shader(u32 gl_program) noexcept
        : gl_program(gl_program)
    {}

    scoped_texture_unit::scoped_texture_unit() noexcept {
        for (i32 i = 0; i < g_state.gl_state.max_texture_units; ++i) {
            if (bitutil::check(i, g_state.gl_state.texture_unit_freelist)) {
                bitutil::clearr(i, g_state.gl_state.texture_unit_freelist); 
                this->unit = i;
                return;
            }
        }

        iris::error(error_code::texture_units_exhausted);
    }

    u32 scoped_texture_unit::operator()() const noexcept {
        return this->unit;
    }

    scoped_texture_unit::~scoped_texture_unit() {
        bitutil::setr(this->unit, g_state.gl_state.texture_unit_freelist);
    }

    object::object(u32 indices, u32 vao, u32 vbo, u32 ebo, struct shader shader) noexcept
        : shader(shader)
        , indices(indices)
        , vao(vao)
        , vbo(vbo)
        , ebo(ebo)
    {
        if (vao == 0 || vbo == 0 || ebo == 0) {
            error(error_code::invalid_gl_object, std::format("One of the OpenGL objects were invalid: (vao, vbo, ebo) {}, {}, {}", vao, vbo, ebo));
            std::exit(0);
        }
    }

    void shader::update_uniforms() const noexcept {
        glUseProgram(this->gl_program);

        for (const auto &[name, value] : uniforms) {
            const GLint location = glGetUniformLocation(this->gl_program, name.c_str());

            if (location == -1) {
                continue;
            }

            std::visit([location](const auto &v) {
                using T = std::decay_t<decltype(v)>;

                if constexpr (std::is_same_v<T, f32>) {
                    glUniform1f(location, v);
                } else if constexpr (std::is_same_v<T, glm::vec2>) {
                    glUniform2fv(location, 1, glm::value_ptr(v));
                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                    glUniform3fv(location, 1, glm::value_ptr(v));
                } else if constexpr (std::is_same_v<T, glm::vec4>) {
                    glUniform4fv(location, 1, glm::value_ptr(v));
                } else if constexpr (std::is_same_v<T, i32>) {
                    glUniform1i(location, v);
                } else if constexpr (std::is_same_v<T, glm::i32vec2>) {
                    glUniform2iv(location, 1, glm::value_ptr(v));
                } else if constexpr (std::is_same_v<T, glm::i32vec3>) {
                    glUniform3iv(location, 1, glm::value_ptr(v));
                } else if constexpr (std::is_same_v<T, glm::i32vec4>) {
                    glUniform4iv(location, 1, glm::value_ptr(v));
                }
            }, value);
        }
    }

    object create_object(const std::vector<float> &vertices, const std::vector<u32> &indices, shader shader, i32 stride_size, i32 draw_type) noexcept {
        u32 vao;
        u32 vbo;
        u32 ebo;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), draw_type);

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32), indices.data(), draw_type);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride_size * sizeof(float), 0);
        glEnableVertexAttribArray(0);
        // glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride_size, 0);
        // glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        return object(indices.size(), vao, vbo, ebo, shader);
    }

    void deinit() noexcept {}
}
