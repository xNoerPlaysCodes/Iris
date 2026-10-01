#include "nutils/types.hpp"
#include <exception>
#include <iris/runtime.hpp>
#include <string>
#include <native.hpp>
#include <gl.h>

// My hatred for windows.h is eternal
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX

#include <intrin.h>
#include <string>
#include <format>

namespace {
    void append_utf16_to_utf8(std::wstring_view src, std::string &dst) {
        dst.reserve(dst.size() + src.size() * 3);
        auto put = [&dst](u32 byte) { dst.push_back(static_cast<char>(byte & 0xFF)); };

        for (size_t i = 0; i < src.size(); i++) {
            u32 wc = src[i];
            if (wc == 0) break;

            if (wc >= 0xD800 && wc <= 0xDFFF) {
                if (wc <= 0xDBFF && i + 1 < src.size()) {
                    u32 wc2 = src[i + 1];
                    if (wc2 >= 0xDC00 && wc2 <= 0xDFFF) {
                        u32 codepoint = 0x10000 + (((wc - 0xD800) << 10) | (wc2 - 0xDC00));
                        i++;

                        put(0xF0 | ((codepoint >> 18) & 0x3F));
                        put(0x80 | ((codepoint >> 12) & 0x3F));
                        put(0x80 | ((codepoint >> 6) & 0x3F));
                        put(0x80 | (codepoint & 0x3F));
                        continue;
                    }
                }
                // invalid surrogate: replace with '?'
                put('?');
                continue;
            }

            // normal code unit
            u32 codepoint = wc;
            if (codepoint < 0x80) {
                put(codepoint);  // ASCII MY BELOVED
            } else if (codepoint < 0x800) {
                put(0xC0 | (codepoint >> 6));
                put(0x80 | (codepoint & 0x3F));
            } else {
                put(0xE0 | (codepoint >> 12));
                put(0x80 | ((codepoint >> 6) & 0x3F));
                put(0x80 | (codepoint & 0x3F));
            }
        }

        return;
    }
}

namespace iris::gl {
    void gl_init() noexcept {
        glewExperimental = true;
        if (GLenum err = glewInit(); err != GLEW_OK) {
            iris::crash(
                std::format(
                    "OpenGL function loading failed — glewInit failed: {}",
                    reinterpret_cast<const char*>(glewGetErrorString(err))
                )
            );
        }
    }
}

namespace iris::native {
    void init() noexcept {
        SetConsoleOutputCP(CP_UTF8); // Windows is weird
    }

    void terminate() noexcept {
        std::terminate();
    }

    bool keyboard_connected() noexcept {
        return false;
    }

    bool mouse_connected() noexcept {
        return false;
    }

    bool touch_connected() noexcept {
        return false;
    }

    std::string device_name() noexcept {
        static_assert(sizeof(wchar_t) == 2);

        WCHAR name[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = static_cast<DWORD>(std::size(name));
        
        if (!GetComputerNameW(name, &size)) return "";

        std::string computerName{};
        append_utf16_to_utf8(name, computerName);
        return computerName;
    }

    std::string processor_name() noexcept {
        int cpu_info[4] = {};
        __cpuid(cpu_info, 0x80000000);
        u32 max_ext_id = static_cast<u32>(cpu_info[0]);
        if (max_ext_id < 0x80000004) return "";

        int regs[12] = {};
        __cpuid(regs + 0, 0x80000002);
        __cpuid(regs + 4, 0x80000003);
        __cpuid(regs + 8, 0x80000004);

        char brand[sizeof(regs) + 1] = {};
        std::memcpy(brand, regs, sizeof(regs));

        std::string name(brand);
        name.erase(0, name.find_first_not_of(' ')); // Intel pads the start with spaces
        return name;
    }

    glm::ivec2 screen_resolution() noexcept {
        return {
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN)
        };
    }
}
