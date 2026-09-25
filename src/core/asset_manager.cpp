#include <chrono>
#include <fstream>
#include <iris/core/asset_manager.hpp>
#include <optional>
#include <stop_token>
#include <thread>
#include "assert.hpp"
#include "error.hpp"
#include "lib/stb_image.h"

namespace iris {
    asset_manager::asset_manager(struct config config) noexcept {
        if (config.auto_unload) {
            this->cleanup_thread = std::jthread([&](std::stop_token st) -> void {
                while (!st.stop_requested()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            });
        }
        this->config = config;
    }

    void asset_manager::intl_load_image(const std::vector<u8> &data, const std::string &key) noexcept {
        {
            std::lock_guard<std::mutex> _(this->registered_images_mutex);
            if (this->registered_images.find(key) != this->registered_images.end()) {
                iris::error(error_code::duplicate, std::format("{} is already a registered image", key));
                return;
            }
        }

        i32 width, height;
        i32 channels;
        u8 *pixel_data = stbi_load_from_memory(data.data(), data.size(), &width, &height, &channels, 0);

        IrisAssert(pixel_data != nullptr);

        if (channels < 4) {
            u8 *new_pixel_data = new u8[width * height * 4];
            for (size_t i = 0; i < (width * height); ++i) {
                new_pixel_data[i*4+0] = pixel_data[i*3+0];
                new_pixel_data[i*4+1] = pixel_data[i*3+1];
                new_pixel_data[i*4+2] = pixel_data[i*3+2];
                new_pixel_data[i*4+3] = 255;
            }

            stbi_image_free(pixel_data);
            pixel_data = new_pixel_data;
            channels = 4;
        }

        struct image image = {
            .width = u32(width),
            .height = u32(height),
            .channels = u32(channels)
        };
        size_t sz = width * height * channels;
        image.data.reserve(sz);
        image.data.insert(image.data.begin(), pixel_data, pixel_data + sz);

        {
            std::lock_guard<std::mutex> _(this->registered_images_mutex);
            this->registered_images[key] = std::move(image);
        }
    }

    void asset_manager::load_image(const std::vector<u8> &data, const std::string &key) noexcept {
        if (data.size() == 0) {
            iris::error(error_code::malformed_input, "Input data vector size() was 0 bytes");
            return;
        }

        this->intl_load_image(data, key);
    }

    void asset_manager::load_image(const std::filesystem::path &path, const std::string &key) noexcept {
        if (!std::filesystem::exists(path)) {
            iris::error(error_code::file_not_found, path.string());
            return;
        }

        std::ifstream file(path);

        if (!file.is_open()) {
            iris::error(error_code::filesystem_error);
            return;
        }

        file.seekg(0, std::ios::end);
        std::vector<u8> data;
        data.resize(file.tellg());
        file.seekg(0, std::ios::beg);

        file.read((char*) data.data(), data.size());
        file.close();

        this->intl_load_image(data, key);
    }

    std::optional<std::reference_wrapper<image>> asset_manager::image(const std::string &key) noexcept {
        std::lock_guard<std::mutex> _(this->registered_images_mutex);
        if (this->registered_images.find(key) == this->registered_images.end()) {
            iris::error(error_code::malformed_input, std::format("{} was not registered as a image", key));
            return std::nullopt;
        }

        return this->registered_images.at(key);
    }

    asset_manager::~asset_manager() {
        this->cleanup_thread.request_stop();
        if (this->cleanup_thread.joinable()) {
            this->cleanup_thread.join();
        }
    }
}
