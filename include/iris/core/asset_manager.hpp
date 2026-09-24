#pragma once

#include "nutils/types.hpp"
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <optional>

namespace iris {
    struct image {
        std::vector<u8> data;
        u32 width = 0;
        u32 height = 0;
        u32 channels = 0;
    };

    struct texture {
        friend constexpr bool operator==(texture, u32) noexcept;
        friend constexpr bool operator==(u32, texture) noexcept;

        friend constexpr bool operator!=(texture, u32) noexcept;
        friend constexpr bool operator!=(u32, texture) noexcept;
    public:
        static constexpr u32 invalid = 0;
    private:
        u32 gl_id = invalid;
    public:
        constexpr u32& operator()() noexcept {
            return this->gl_id;
        }

        constexpr u32 view() const noexcept {
            return this->gl_id;
        }

        constexpr texture& operator=(u32 val) noexcept {
            this->gl_id = val;
            return *this;
        }

        constexpr bool is_valid() const noexcept {
            return this->gl_id != 0;
        }

        constexpr texture() = default;
        constexpr texture(u32 val) : gl_id(val) {}
        constexpr ~texture() = default;
    };

    constexpr bool operator==(texture lhs, u32 rhs) noexcept {
        return lhs.gl_id == rhs;
    }

    constexpr bool operator==(u32 lhs, texture rhs) noexcept {
        return lhs == rhs.gl_id;
    }

    constexpr bool operator!=(texture lhs, u32 rhs) noexcept {
        return lhs.gl_id != rhs;
    }

    constexpr bool operator!=(u32 lhs, texture rhs) noexcept {
        return lhs != rhs.gl_id;
    }

    class asset_manager {
    public:
        struct config {
            bool auto_unload : 1;

            config() noexcept
                : auto_unload(false)
            {}
        };
    private:
        std::jthread cleanup_thread;

        std::mutex registered_images_mutex;
        std::unordered_map<std::string, image> registered_images;
        config config;

        void intl_load_image(const std::vector<u8> &data, const std::string &key) noexcept;
    public:
        void load_image(const std::filesystem::path &path, const std::string &key) noexcept;
        void load_image(const std::vector<u8> &data, const std::string &key) noexcept;
        std::optional<std::reference_wrapper<image>> image(const std::string &key) noexcept;

        asset_manager(struct config config = {}) noexcept;
        ~asset_manager();
    };
}
