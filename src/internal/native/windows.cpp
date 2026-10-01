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
    size_t utf16_to_utf8(const std::wstring &src, std::string &dst) {
        size_t di = 0;
        dst.resize(src.size() * 3);

        for (size_t i = 0; i < src.size(); i++) {
            u16 wc = src[i];
            if (wc == 0) break;

            if (wc >= 0xD800 && wc <= 0xDFFF) {
                if (wc <= 0xDBFF && i + 1 < src.size()) {
                    u16 wc2 = src[i + 1];
                    if (wc2 >= 0xDC00 && wc2 <= 0xDFFF) {
                        u32 codepoint = 0x10000 + (((wc - 0xD800) << 10) | (wc2 - 0xDC00));
                        i++;

                        dst[di++] = 0xF0 | (codepoint >> 18);
                        dst[di++] = 0x80 | ((codepoint >> 12) & 0x3F);
                        dst[di++] = 0x80 | ((codepoint >> 6) & 0x3F);
                        dst[di++] = 0x80 | (codepoint & 0x3F);
                        continue;
                    }
                }
                // invalid surrogate: replace with '?'
                dst[di++] = '?';
                continue;
            }

            // normal code unit
            uint32_t codepoint = wc;
            if (codepoint < 0x80) {
                dst[di++] = static_cast<char>(codepoint);  // ASCII MY BELOVED
            } else if (codepoint < 0x800) {
                dst[di++] = 0xC0 | (codepoint >> 6);
                dst[di++] = 0x80 | (codepoint & 0x3F);
            } else {
                dst[di++] = 0xE0 | (codepoint >> 12);
                dst[di++] = 0x80 | ((codepoint >> 6) & 0x3F);
                dst[di++] = 0x80 | (codepoint & 0x3F);
            }
        }

        dst.resize(di); // Took me a while to realize, size is last_idx+1
        return di;
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

        std::wstring computerNameWide{name, size};
        std::string computerName{};
        utf16_to_utf8(computerNameWide, computerName);
        return computerName;
    }

    std::string processor_name() noexcept {
        int cpu_info[4] = {};
        alignas(int) char brand[0x40] = {};

        __cpuid(cpu_info, 0x80000000);
        unsigned int max_ext_id = static_cast<unsigned int>(cpu_info[0]);
        if (max_ext_id < 0x80000004) {
            return "";
        }

        __cpuid(reinterpret_cast<int*>(brand), 0x80000002);
        __cpuid(reinterpret_cast<int*>(brand + 16), 0x80000003);
        __cpuid(reinterpret_cast<int*>(brand + 32), 0x80000004);

        return std::string(brand);
    }

    glm::ivec2 screen_resolution() noexcept {
        return {
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN)
        };
    }
}
