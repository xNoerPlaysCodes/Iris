#include <glm/glm.hpp>
#include "iris/graphics/rendering.hpp"
#include "iris/core/asset_manager.hpp"
#include "iris/graphics/windowing.hpp"
#include "iris/io/io.hpp"
#include "iris/types.hpp"
#include <chrono>
#include <thread>
#include <iris/runtime.hpp>
#include <iostream>

void iris_main(const iris_main_arguments &) {
    auto init_data = iris::init();

    iris::window window = { "Iris — Core Window Test", { 800, 600 } };
    iris::renderer renderer(window);
    iris::asset_manager am;

    am.load_image("bin/resources/noer.png", "test");
    auto noerPfp = am.image("test");

    if (!noerPfp.has_value())
        iris::crash("No noerlol found :C. Please get noer :D.");
    iris::texture tx = renderer.load_texture(noerPfp->get());
    double timeX = 0;
    auto frameStart = std::chrono::steady_clock::now();

    while (window.running()) {
        auto frameNow = std::chrono::steady_clock::now();
        auto dt = std::chrono::duration<double>(
            frameNow - frameStart
        ).count();
        frameStart = std::chrono::steady_clock::now();
        dt = std::min(dt, 0.1);

        if (!window.visible_surface()) {
            std::cout << "Not visible\n";
            window.poll_events(); // Bro you forgor this :skull:
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        renderer.begin_frame();
        renderer.clear();
        {
            static glm::vec2 pos = { 256, 256 };
            renderer.draw_rectangle({ 0, 700 }, { 256, 256 }, { 255, 0, 255, 255 });
            renderer.draw_texture(pos, { 256, 256 }, tx);
            renderer.draw_rectangle({ 0, 300 }, { 256, 256 }, { 255, 0, 255, 255 });
            if (window.key_state(iris::io::key::w).down()) {
                pos.y -= 60.f * dt;
            }
            if (window.key_state(iris::io::key::s).down()) {
                pos.y += 60.f * dt;
            }
            if (window.key_state(iris::io::key::a).down()) {
                pos.x -= 60.f * dt;
            }
            if (window.key_state(iris::io::key::d).down()) {
                pos.x += 60.f * dt;
            }
        }
        renderer.end_frame();
        window.poll_events();
        window.swap_buffers();
        frameNow = std::chrono::steady_clock::now();
    }
}

IrisPlatformGlue
