#pragma once

#include "nutils/types.hpp"
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <string_view>
#include "iris/platform_macros.hpp"

#ifdef Iris_Platform_Android
#include "android_native_app_glue.h"
#endif

namespace iris {
    constexpr bool dont_log = false;

    /// @brief Log Hook
    /// @note return true if you want the log to be sent
    ///       through the normal logger path, else return
    ///       false
    using log_hook = std::function<bool(
        spdlog::level_t level,
        const std::string &message
    )>; 

    /// @brief Platform Representation Structure
    struct platform {
        constexpr static u32 cap_keyboard = 0x1;
        constexpr static u32 cap_mouse = 0x2;
        constexpr static u32 cap_touch = 0x4;

        glm::u32vec2 screen_resolution;
        u32 cpus = 0;
        u32 capabilities = 0;
        std::string device_name;
        std::string processor; 
        std::string pretty_name;
    };

#define Iris_Error_Codes       \
    X(unknown_error) \
    X(native_error) \
    X(texture_units_exhausted) \
    X(outdated_gpu_driver) \
    X(invalid_configuration) \
    X(already_initialized) \
    X(memory_allocation_failure) \
    X(unrecognized_cli_argument) \
    X(invalid_gl_object) \
    X(gl_error) \
    X(missing_shader_uniform_location) \
    X(incapable_hardware) \
    X(invalid_asset_manager_key) \
    X(file_not_found) \
    X(filesystem_error) \
    X(malformed_input) \
    X(duplicate) \
    X(unimplemented)

#define X(name) name,

    enum class error_code : u32 {
        null = 0,
        Iris_Error_Codes
    };

#undef X
#define X(name) case error_code::name: return #name;

    namespace detail {
        [[nodiscard]] inline constexpr std::string_view error_code_to_string(
            error_code error
        ) noexcept {
            switch (error) {
                default: return "null";
                Iris_Error_Codes
            }
        }
    }

#undef X
#undef Iris_Error_Codes 

    /// @brief Error hook
    /// @note return true if you want the log to be sent
    ///       through the normal logger path, else return
    ///       false
    using error_hook = std::function<bool(
        error_code ec,
        const std::string &additional_message
    )>;
}

namespace iris {
    /// @brief Initialize IrisGE
    /// @note If you are using iris_main then there is no need
    ///       to pass anything as the arguments of this function
    /// @note REQUIRED! Do not use any other part of the 
    ///       IrisGE API before calling this
    [[nodiscard]] platform init(int argc = 0, char *argv[] = nullptr) noexcept;

    /// @brief Hook the logger
    void hook_log(log_hook hook) noexcept;

    /// @brief Hook erroring
    void hook_error(error_hook hook) noexcept;

    /// @brief Crash fatally
    [[noreturn]] void crash(const std::string &message = "Fatal Crash") noexcept;

    namespace detail {
        void pre_main() noexcept;
        void init_set_arguments(int argc, char **argv) noexcept;
    }
}

struct iris_main_arguments {};

void iris_main(const iris_main_arguments &args);

#ifdef Iris_Platform_Desktop
#define IrisPlatformGlue \
    int main(int argc, char **argv) { \
        iris::detail::pre_main(); \
        iris::detail::init_set_arguments(argc, argv); \
        iris_main({}); \
    }
#else
#define IrisPlatformGlue \
    android_app *g_android_app; \
    extern "C" void android_main(android_app *app) { \
        g_android_app = app; \
        app->onAppCmd = [](android_app *app, int32_t cmd) {}; \
            ANativeActivity_setWindowFlags(app->activity, \
            0, 0); \
        while (app->window == nullptr) { \
            int events; \
            android_poll_source *src; \
            ALooper_pollOnce(100, nullptr, &events, (void**)&src); \
            if (src != nullptr) src->process(app, src); \
            if (app->destroyRequested) return; \
        } \
        iris::detail::pre_main(); \
        char *args[] = { (char*) "program", nullptr }; \
        iris::detail::init_set_arguments(0, args); \
        iris_main({}); \
    }
#endif

// #ifdef PC
// #define IrisMain int main()
// #define IrisMainReturn return 0
//
// IrisMain {
//     IrisMainReturn;
// }
// #else
// #define IrisMain void iris_platform_main()
// #define IrisMainReturn return
//
// IrisMain {
//     IrisMainReturn;
// }
// #endif
