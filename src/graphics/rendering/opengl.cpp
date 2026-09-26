#include "error.hpp"
#include "glm/fwd.hpp"
#include "iris/core/asset_manager.hpp"
#include "iris/runtime.hpp"
#include "iris/types.hpp"
#include <cstddef>
#include <fstream>
#include <iris/graphics/rendering.hpp>
#include "gl_render_layer.hpp"
#include "nutils/types.hpp"
#include "spdlog/spdlog.h"
#include <assert.hpp>
#include <string>
#include <type_traits>
#include "lib/stb_truetype.h"
#include "state.hpp"

namespace iris {
    namespace {
        struct gl_resources {
            gl::object obj_quad;
        };

        glm::vec4 rgba_color_to_vec4(rgba_color col) noexcept {
            return {
                static_cast<float>(col.r) / 255,
                static_cast<float>(col.g) / 255,
                static_cast<float>(col.b) / 255,
                static_cast<float>(col.a) / 255,
            };
        }
    }

    struct instance {
        glm::vec2 pos;
        glm::vec2 size;
        glm::vec4 color;
    };

    struct renderer::drawcall {
        std::unordered_map<std::string, gl::uniform_value> uniforms;
        instance instance;
        gl::object &object;
        u32 texture = 0;
    };

    renderer::renderer(const class window &window, struct config cfg) noexcept
        : window(window) 
    {
        this->config.gles = window.config.gles;
        this->resources = new gl_resources();
        window.make_gl_context_current();
        gl::init({
            .viewport_size = cfg.viewport_size_override != glm::vec2 { -1, -1 } 
                            ? cfg.viewport_size_override 
                            : glm::vec2 { 0, 0 },

            .viewport_offset = cfg.viewport_offset_override != glm::vec2 { -1, -1 } 
                            ? cfg.viewport_offset_override 
                            : glm::vec2 { 0, 0 },

            .depth_func = this->config.depth 
                            ? gl::depth_func::less
                            : gl::depth_func::none,
        });
        if (!this->config.gles) {
            glEnable(0x809D /* GL_MULTISAMPLE, not defined on GL ES tho, and I don't want to use more macros */);
        }
        std::string shader_version_string;
        if (this->config.gles) {
            shader_version_string = "#version 300 es\n";
        } else {
            shader_version_string = "#version 330 core\n";
        }
        gl::shader shader = gl::compile_debug_shader(shader_version_string);
        shader.update_uniforms();
        reinterpret_cast<gl_resources*>(this->resources)->obj_quad = gl::create_object({ 0, 0, 1, 0, 1, 1, 0, 1 }, { 0, 1, 2, 2, 3, 0, }, shader);
    }

    void renderer::begin_frame() noexcept {
        this->begin_frame_called = true;
    }

    void renderer::end_frame() noexcept {
        this->viewport_size_ = this->window.framebuffer_size();
        this->viewport_offset_ = { 0, 0 };
        glm::ivec2 vp_offset = this->config.viewport_offset_override != glm::vec2 { -1, -1 } ? this->config.viewport_offset_override : this->viewport_offset_;
        glm::ivec2 vp_size = this->config.viewport_size_override != glm::vec2 { -1, -1 } ? this->config.viewport_size_override : this->viewport_size_;
        glViewport(vp_offset.x, vp_offset.y, vp_size.x, vp_size.y);

        this->flush_drawcalls(); // actually draw

        this->begin_frame_called = !this->begin_frame_called;
        IrisAssert(this->begin_frame_called == false);
    }

    void renderer::pre_draw_check() const noexcept {
        IrisAssert(this->begin_frame_called);
    }

    void renderer::flush_drawcalls() noexcept {
        for (auto &dc : this->drawcalls) {
            glUseProgram(dc.object.shader.gl_program);

            // dc.object.shader.uniforms.try_emplace("p_pos", glm::vec2{});
            // if (auto &val = std::get<glm::vec2>(dc.object.shader.uniforms.at("p_pos"));
            //     dc.instance.pos != val)
            // {
            //     val = dc.instance.pos;
            //     dc.object.shader.update_uniforms();
            // }
            //
            // dc.object.shader.uniforms.try_emplace("p_size", glm::vec2{});
            // if (auto &val = std::get<glm::vec2>(dc.object.shader.uniforms.at("p_size"));
            //     dc.instance.size != val)
            // {
            //     val = dc.instance.size;
            //     dc.object.shader.update_uniforms();
            // }
            //
            // dc.object.shader.uniforms.try_emplace("p_color", glm::vec4{});
            // if (auto &val = std::get<glm::vec4>(dc.object.shader.uniforms.at("p_color"));
            //     dc.instance.color != val)
            // {
            //     val = rgba_color_to_vec4(dc.instance.color);
            //     dc.object.shader.update_uniforms();
            // }

            for (auto &[k, v] : dc.uniforms) {
                dc.object.shader.uniforms.try_emplace(k, std::remove_cvref_t<decltype(v)>{});
                if (auto &val = dc.object.shader.uniforms.at(k); val != v) {
                    val = v;
                    dc.object.shader.update_uniforms();
                }
            }

            gl::scoped_texture_unit unit;

            if (dc.texture != 0) {
                glActiveTexture(GL_TEXTURE0 + unit());
                glBindTexture(GL_TEXTURE_2D, dc.texture);

                dc.object.shader.uniforms.try_emplace("p_texture", i32{});
                dc.object.shader.uniforms.try_emplace("p_texture_provided", i32{});
                if (auto &val = std::get<i32>(dc.object.shader.uniforms.at("p_texture"));
                    i32(unit()) != val)
                {
                    val = i32(unit());
                    dc.object.shader.update_uniforms();
                }

                if (auto &val = std::get<i32>(dc.object.shader.uniforms.at("p_texture_provided"));
                    val != 1)
                {
                    val = 1;
                    dc.object.shader.update_uniforms();
                }
            }

            glBindVertexArray(dc.object.vao);
            // TODO: instance it acutally
            std::vector<instance> instances = { dc.instance };
            glBindBuffer(GL_ARRAY_BUFFER, dc.object.vbo);
            glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(instance), nullptr, GL_STREAM_DRAW);  // orphan
            glBufferSubData(GL_ARRAY_BUFFER, 0, instances.size() * sizeof(instance), instances.data());
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_TRUE, 0, &instances[0].pos);

            glVertexAttribPointer(2, 2, GL_FLOAT, GL_TRUE, 0, &instances[0].size);
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, instances.size());

            glActiveTexture(GL_TEXTURE0);
        }
        this->drawcalls.clear();
    }

    void renderer::clear(rgba_color color) noexcept {
        pre_draw_check();
        glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
        i32 clear_flags = GL_COLOR_BUFFER_BIT;
        if (this->config.depth) {
            clear_flags |= GL_DEPTH_BUFFER_BIT;
        }
        glClear(clear_flags);
    }

    glm::vec2 renderer::viewport_size() const noexcept {
        return this->viewport_size_;
    }

    glm::vec2 renderer::viewport_offset() const noexcept {
        return this->viewport_offset_;
    }

    void renderer::draw_fps(glm::vec2 pos) noexcept {
        (void) pos;
    }

#ifdef Iris_Debug
    void renderer::debug() noexcept {
        pre_draw_check();
    }
#endif

    texture renderer::load_texture(const image &image, u32 filter) const noexcept {
        texture tx = texture::invalid;
        glGenTextures(1, &tx());
        glBindTexture(GL_TEXTURE_2D, tx());
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data.data());
        
        if (filter & filter_linear) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else if (filter & filter_nearest_neighbour) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        } else {
            iris::error(error_code::malformed_input, "Filter bitmask is invalid");
            glBindTexture(GL_TEXTURE_2D, internal::g_state.gl_state.bound_texture);
            glDeleteTextures(1, &tx());
            return texture::invalid;
        }

        glBindTexture(GL_TEXTURE_2D, internal::g_state.gl_state.bound_texture);
        return tx;
    }

    void renderer::unload_texture(texture &tx) const noexcept {
        if (tx == texture::invalid) {
            iris::error(error_code::malformed_input, "Texture is invalid");
            return;
        }

        if (internal::g_state.gl_state.bound_texture == tx()) {
            glBindTexture(GL_TEXTURE_2D, 0);
            internal::g_state.gl_state.bound_texture = 0;
        }

        glDeleteTextures(1, &tx());
    }

    void renderer::draw_rectangle(glm::vec2 pos, glm::vec2 size, rgba_color color) noexcept {
        pre_draw_check();

        gl_resources *res = reinterpret_cast<gl_resources*>(this->resources);

        drawcall dc = {
            .instance = {
                .pos = pos / this->viewport_size_,
                .size = size / this->viewport_size_,
                .color = rgba_color_to_vec4(color)
            },
            .object = res->obj_quad,
        };

        this->drawcalls.push_back(std::move(dc));
    }

    void renderer::draw_texture(glm::vec2 pos, glm::vec2 size, const texture &tx) noexcept {
        pre_draw_check();

        gl_resources *res = reinterpret_cast<gl_resources*>(this->resources);

        drawcall dc = {
            .instance = {
                .pos = pos / this->viewport_size_,
                .size = size / this->viewport_size_,
                .color = { 0, 0, 0, 0 }
            },
            .object = res->obj_quad,
            .texture = tx.view(),
        };

        this->drawcalls.push_back(std::move(dc));
    }

    renderer::~renderer() {
        delete reinterpret_cast<gl_resources*>(this->resources);
    }
}
