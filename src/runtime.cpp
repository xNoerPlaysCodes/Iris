#include "crash_handler.hpp"
#include "error.hpp"
#include "native.hpp"
#include "spdlog/spdlog.h"
#include "utilities.hpp"
#include <cstddef>
#include <iostream>
#include <iris/runtime.hpp>
#include <string>
#include <state.hpp>
#include <assert.hpp>
#include <nutils/types.hpp>

#define Iris_Args \
     X(fast-startup, "Enables fast-startup by disabling some startup checks\n", flags.fast_startup) \
     X(no-input-capability-check, "Disables startup input capabilities check (returns false on everything)\n", flags.no_input_capability_check)

namespace {
    struct cli_flags {
        bool fast_startup : 1 = false;
        bool no_input_capability_check = false;
    };

    cli_flags parse_cli(int argc, char *argv[]) noexcept {
        static bool parsed = false;
        static cli_flags flags;
        if (parsed) {
            return flags;
        }
        for (int i = 0; i < argc; ++i) {
            std::string arg = argv[i];
            std::string value;

            if (!arg.starts_with("--")) {
                continue;
            }

            arg = arg.substr(2);

            if (/* requires value */ false && i + 1 < argc) {
                value = argv[i + 1];
            }

            spdlog::trace("arg: {}", arg);
            if (arg == "help") {
#define X(x, y, z) #x ": " y
                std::cout << Iris_Args;
                std::exit(0);
#undef X
            }
#undef X
#define X(x, y, z) else if (arg == #x) {\
    z = true; \
} \

            Iris_Args
#undef X
#undef Iris_Args
            else {
                iris::error(iris::error_code::unrecognized_cli_argument, std::format("{} with value (if any) '{}'", arg, value));
            }
        }
        parsed = true;
        return flags;
    }
}

namespace iris {
    namespace detail {
        void init_set_arguments(int argc, char **argv) noexcept {
            parse_cli(argc, argv);
        }

        void pre_main() noexcept {
            static bool ran = false;
            if (ran) return;
            ran = true;
            native::init();
            spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
#ifdef Iris_Debug
            spdlog::set_level(spdlog::level::trace);
#else
            spdlog::set_level(spdlog::level::info);
#endif
            spdlog::flush_on(spdlog::level::trace);
        }
    }
    platform init(int argc, char *argv[]) noexcept {
        detail::pre_main();
        static bool initialized = false;
        if (initialized) {
            iris::error(error_code::already_initialized, "iris::init() called more than once");
            std::exit(1);
        }
        initialized = true;
        if (argc > 0) {
            IrisAssert(argv != nullptr);
        }
        util::timer_t timer;
        cli_flags flags = parse_cli(argc, argv);
        u32 keyboard_bitmask = 0;
        u32 mouse_bitmask = 0;
        u32 touch_bitmask = 0;
        if (!flags.no_input_capability_check) {
            if (native::keyboard_connected()) {
                keyboard_bitmask = iris::platform::cap_keyboard;
            } else if (native::mouse_connected()) {
                mouse_bitmask = iris::platform::cap_mouse;
            } else if (native::touch_connected()) {
                touch_bitmask = iris::platform::cap_touch;
            }
        }
        platform ret = {
            .screen_resolution = native::screen_resolution(),
            .cpus = std::thread::hardware_concurrency(),
            .capabilities = keyboard_bitmask | mouse_bitmask | touch_bitmask,
            .device_name = flags.fast_startup ? "Disabled due to Fast Startup" : native::device_name(),
            .processor = native::processor_name(),
        };

        ret.pretty_name = std::format("{} — {} ({})", ret.device_name, ret.processor, ret.cpus);

        timer.stop();
        
        spdlog::debug("Device: {}", ret.device_name);
        spdlog::debug("Processor: {} ({})", ret.processor, ret.cpus);
        spdlog::debug("Pretty Name: {}", ret.pretty_name);
        spdlog::debug("Screen Resolution: {}x{}", 
            ret.screen_resolution.x,
            ret.screen_resolution.y
        );
        spdlog::debug("Capabilities:");
        spdlog::debug("  Keyboard: {}", static_cast<bool>(ret.capabilities & iris::platform::cap_keyboard));
        spdlog::debug("  Mouse: {}", static_cast<bool>(ret.capabilities & iris::platform::cap_mouse));
        spdlog::debug("  Touch: {}", static_cast<bool>(ret.capabilities & iris::platform::cap_touch));

        spdlog::debug("Initialized platform layer in {}", timer.to_str());

        return ret;
    }

    void hook_log(log_hook hook) noexcept {
        internal::g_state.log_hook = hook;
        iris::error(error_code::unimplemented);
    }

    void hook_error(error_hook hook) noexcept {
        internal::g_state.error_hook = hook;
    }

    void crash(const std::string &message) noexcept {
        internal::crash(message);
        native::terminate();
    }
}
