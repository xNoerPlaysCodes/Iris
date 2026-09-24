#include "iris/runtime.hpp"
#include "state.hpp"
#include <error.hpp>
#include <string>

namespace iris {
    void error(iris::error_code ec, const std::string &msg) noexcept {
        if (auto hook = internal::g_state.error_hook; hook != nullptr) {
            if (hook(ec, msg) == dont_log) {
                return;
            }
        }
        std::string additional = msg.empty() ? "" : " — " + msg;
        spdlog::error("An error occurred: {}{}", detail::error_code_to_string(ec), additional);
    }
}
