#pragma once

#include <iris/runtime.hpp>
#include <limits>
#include <spdlog/spdlog.h>
#include <gl_render_layer.hpp>

#define Iris_EncodeGLVersion(major, minor) ((major * 10) + (minor))
#define Iris_DecodeGLVersionMajor(encoded) ((encoded) / 10)
#define Iris_DecodeGLVersionMinor(encoded) ((encoded) % 10)

namespace iris::internal {
    struct global_state {
        struct {
            u32 bound_framebuffer = 0;
            u32 bound_texture = 0;
            u32 texture_unit_freelist = 0;
            i32 max_texture_units = 0;
            u32 gl_version = 0;
        } gl_state;
        log_hook log_hook = nullptr;
        error_hook error_hook = nullptr;
    };

    extern global_state g_state;
}
