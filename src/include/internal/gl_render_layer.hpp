#pragma once

#include <glm/glm.hpp>
#include "nutils/types.hpp"
#include <glm/glm.hpp>
#include <gl.h>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace iris::gl {
    enum class depth_func : u32 {
        none = 0,
        less,
        equal,
        greater,
    };

    struct init_config {
        glm::vec2 viewport_size;
        glm::vec2 viewport_offset;
        depth_func depth_func = depth_func::none;
    }; 

    struct scoped_texture_unit {
    private:
        u32 unit = 0;
    public:
        scoped_texture_unit() noexcept;
        u32 operator()() const noexcept;
        ~scoped_texture_unit();
    };

    using uniform_value = std::variant<
        f32,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        i32,
        glm::i32vec2,
        glm::i32vec3,
        glm::i32vec4
    >;

    struct shader {
    public:
        std::unordered_map<std::string, uniform_value> uniforms;
        u32 gl_program = 0;
    public:
        void update_uniforms() const noexcept;
    public:
        shader(u32 gl_program) noexcept;
        shader() noexcept = default;
        ~shader() = default;
    };

    struct object {
    public:
        shader shader;
        u32 indices = 0;
        u32 vao = 0;
        u32 vbo = 0;
        u32 ebo = 0;
    public:
        object(u32 indices, u32 vao, u32 vbo, u32 ebo, struct shader shader) noexcept;
        object() noexcept = default;
        ~object() = default;        
    };

    u32 compile_debug_shader(std::string shader_version_str) noexcept;

    object create_object(const std::vector<float> &vertices, const std::vector<u32> &indices, shader shader, i32 stride_size = 2, i32 gl_draw_type = GL_STATIC_DRAW) noexcept;

    void check_error(i32 n = -1) noexcept;

    void init(const init_config &cfg) noexcept;
    void deinit() noexcept;
}
