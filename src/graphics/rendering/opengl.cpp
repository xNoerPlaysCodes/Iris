#include "error.hpp"
#include "gl.h"
#include "glm/fwd.hpp"
#include "iris/core/asset_manager.hpp"
#include "iris/runtime.hpp"
#include "iris/types.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iris/graphics/rendering.hpp>
#include "gl_render_layer.hpp"
#include "nutils/types.hpp"
#include "spdlog/spdlog.h"
#include <assert.hpp>
#include <string>
#include <thread>
#include <type_traits>
#include "lib/stb_truetype.h"
#include "state.hpp"

#include "resources/quad_vert.glsl.h"
#include "resources/quad_frag.glsl.h"

#include "resources/fb_blit_vert.glsl.h"
#include "resources/fb_blit_frag.glsl.h"

namespace iris {
    namespace {
        struct gl_resources {
            gl::object obj_quad;
            gl::object obj_fb_blit;
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

    struct renderer::drawcall {
        std::unordered_map<std::string, gl::uniform_value> uniforms;
        gl::instance instance;
        gl::object &object;
        u32 texture = 0;
    };

    renderer::renderer(const class window &window, struct config cfg) noexcept
        : window(window) 
    {
        this->viewport_size_ = window.framebuffer_size();

        this->config.gles = window.config.gles;
        this->resources = new gl_resources();
        gl::init({
            .viewport_size = cfg.viewport_size_override != glm::vec2 { -1, -1 } 
                            ? cfg.viewport_size_override 
                            : this->viewport_size_,

            .viewport_offset = cfg.viewport_offset_override != glm::vec2 { -1, -1 } 
                            ? cfg.viewport_offset_override 
                            : this->viewport_offset_,

            .depth_func = gl::depth_func::less,
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

        gl::framebuffer fb = this->create_framebuffer(this->viewport_size_);
        glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
        glBindTexture(GL_TEXTURE_2D, fb.color_tex);
        internal::g_state.gl_state.default_fb = fb;
        internal::g_state.gl_state.bound_texture = fb.color_tex;

        gl::shader quad_shader = gl::compile_shader(shader_version_string, quad_vert, quad_frag);
        quad_shader.update_uniforms();
        gl_resources *res = reinterpret_cast<gl_resources*>(this->resources);
        res->obj_quad = gl::create_object({ 0, 0, 1, 0, 1, 1, 0, 1 }, { 0, 1, 2, 2, 3, 0, }, quad_shader);
        res->obj_quad.type = gl::object::type::quad;

        gl::shader blit_shader = gl::compile_shader(shader_version_string, fb_blit_vert, fb_blit_frag);
        blit_shader.update_uniforms();

        u32 vao;
        u32 vbo;
        u32 ebo;

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        std::array<float, 8> vertices = { 0, 0, 1, 0, 1, 1, 0, 1 };
        std::array<i32, 6> indices = { 0, 1, 2, 2, 3, 0, };

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);

        glBindVertexArray(0);
        gl::object obj_fb_blit;
        obj_fb_blit.vao = vao;
        obj_fb_blit.vbo = vbo;
        obj_fb_blit.ebo = ebo;
        obj_fb_blit.type = gl::object::type::fb_blit;
        obj_fb_blit.shader = blit_shader;
        obj_fb_blit.indices = 6;
        res->obj_fb_blit = obj_fb_blit;
        res->obj_fb_blit.type = gl::object::type::fb_blit;
    }

    void renderer::begin_frame() noexcept {
        this->begin_frame_called = true;
    }

    gl::framebuffer renderer::create_framebuffer(glm::vec2 size) const noexcept {
        gl::framebuffer fb;
        glGenFramebuffers(1, &fb.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
        glGenTextures(1, &fb.color_tex);
        glBindTexture(GL_TEXTURE_2D, fb.color_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, size.x, size.y, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.color_tex, 0);

        glGenTextures(1, &fb.depth_tex);
        glBindTexture(GL_TEXTURE_2D, fb.depth_tex);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size.x, size.y, 0,
            GL_DEPTH_COMPONENT, GL_FLOAT, NULL
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fb.depth_tex, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            iris::error(error_code::gl_error, "Framebuffer creation failed");
            std::exit(1);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, internal::g_state.gl_state.bound_fb.fbo);
        glBindTexture(GL_TEXTURE_2D, internal::g_state.gl_state.bound_texture);

        fb.size = size;

        return fb;
    }

    void renderer::end_frame() noexcept {
        this->viewport_size_ = this->window.framebuffer_size();
        this->viewport_offset_ = { 0, 0 };
        glm::ivec2 vp_offset = this->config.viewport_offset_override != glm::vec2 { -1, -1 } ? this->config.viewport_offset_override : this->viewport_offset_;
        glm::ivec2 vp_size = this->config.viewport_size_override != glm::vec2 { -1, -1 } ? this->config.viewport_size_override : this->viewport_size_;
       
        if (glm::vec2(vp_size) != internal::g_state.gl_state.default_fb.size) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glDeleteFramebuffers(1, &internal::g_state.gl_state.default_fb.fbo);
            glDeleteTextures(1, &internal::g_state.gl_state.default_fb.color_tex);
            glDeleteTextures(1, &internal::g_state.gl_state.default_fb.depth_tex);

            internal::g_state.gl_state.default_fb = create_framebuffer(this->viewport_size_);

        }

        glBindFramebuffer(GL_FRAMEBUFFER, internal::g_state.gl_state.default_fb.fbo);
        glEnable(GL_DEPTH_TEST);
        glViewport(vp_offset.x, vp_offset.y, vp_size.x, vp_size.y);

        this->flush_drawcalls(); // actually draw

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glViewport(vp_offset.x, vp_offset.y, vp_size.x, vp_size.y);

        gl_resources *res = reinterpret_cast<gl_resources*>(this->resources);

        {
            glDisable(GL_DEPTH_TEST);
            gl::scoped_texture_unit color;

            glActiveTexture(GL_TEXTURE0 + color());
            glBindTexture(GL_TEXTURE_2D, internal::g_state.gl_state.default_fb.color_tex);

            glUseProgram(res->obj_fb_blit.shader.gl_program);
            glUniform1i(glGetUniformLocation(res->obj_fb_blit.shader.gl_program, "p_texture"), color());
            glBindVertexArray(res->obj_fb_blit.vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

            glEnable(GL_DEPTH_TEST);
        }

        this->z_order = max_z_order;

        this->begin_frame_called = !this->begin_frame_called;
        IrisAssert(this->begin_frame_called == false);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void renderer::pre_draw_check() const noexcept {
        IrisAssert(this->begin_frame_called);
        IrisAssert(this->z_order != 0.f);
    }

    void renderer::flush_drawcalls() noexcept {
        std::vector<gl::instance> quad_instances;
        quad_instances.reserve(this->drawcalls.size());

        // BTW: texture instancing rather than solid color instancing
        // should use diff shader sooo thus a different seperate drawcall
        // and IBO
        auto is_instanceable = [](const drawcall &dc) -> bool {
            return dc.object.type == gl::object::type::quad
                && dc.texture == 0; // TODO: Add texture instancing asw!!!
        };

        for (auto &dc : this->drawcalls) {
            if (is_instanceable(dc)) {
                quad_instances.push_back(std::move(dc.instance));
                continue;
            }

            glUseProgram(dc.object.shader.gl_program);

            for (auto &[k, v] : dc.uniforms) {
                dc.object.shader.uniforms.try_emplace(k, std::remove_cvref_t<decltype(v)>{});
                if (auto &val = dc.object.shader.uniforms.at(k); val != v) {
                    val = v;
                    dc.object.shader.update_uniforms();
                }
            }

            gl::scoped_texture_unit unit;

            if (dc.texture == 0) {
                if (auto &val = std::get<i32>(dc.object.shader.uniforms.at("p_texture_provided"));
                    val != 0)
                {
                    val = 0;
                    dc.object.shader.update_uniforms();
                }
            } else {
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
            glBindBuffer(GL_ARRAY_BUFFER, dc.object.inst_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(gl::instance), &dc.instance, GL_STREAM_DRAW);
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, 1);

            glActiveTexture(GL_TEXTURE0);
        }

        this->drawcalls.clear();

        if (quad_instances.empty()) return;

        gl::object &quad_obj = reinterpret_cast<gl_resources*>(this->resources)->obj_quad;
        u32 buffer_size = quad_obj.inst_vbo_size;
        u32 required_buffer_size = sizeof(gl::instance) * quad_instances.size();
        glBindVertexArray(quad_obj.vao);
        glBindBuffer(GL_ARRAY_BUFFER, quad_obj.inst_vbo);
        glBufferData(GL_ARRAY_BUFFER, required_buffer_size, quad_instances.data(), GL_STREAM_DRAW);
        quad_obj.inst_vbo_size = required_buffer_size;
        glUseProgram(quad_obj.shader.gl_program);
        if (auto &val = std::get<i32>(quad_obj.shader.uniforms.at("p_texture_provided"));
            val != 0)
        {
            val = 0;
            quad_obj.shader.update_uniforms();
        }
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, quad_instances.size());
    }

    void renderer::clear(rgba_color color) noexcept {
        pre_draw_check();
        glBindFramebuffer(GL_FRAMEBUFFER, internal::g_state.gl_state.default_fb.fbo);
        glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
                .pos = glm::vec3(pos / this->viewport_size_, (this->z_order--) / max_z_order),
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
                .pos = glm::vec3(pos / this->viewport_size_, (this->z_order--) / max_z_order),
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
