#include "glm/glm.hpp"
#include "iris/graphics/rendering.hpp"
#include "iris/core/asset_manager.hpp"
#include "iris/graphics/windowing.hpp"
#include "iris/io/io.hpp"
#include "iris/types.hpp"
#include <iris/runtime.hpp>

void iris_main(const iris_main_arguments &) {
    auto init_data = iris::init();
    iris::window window = { "Iris — Core Window Test", { 800, 600 } };
    iris::renderer renderer(window);
    iris::asset_manager am;
    am.load_image("bin/resources/noer.png", "test");
    iris::texture tx = renderer.load_texture(*am.image("test"));

    while (window.running()) {
        if (!window.visible_surface()) {
            continue;
        }
        renderer.begin_frame();
        renderer.clear();
        {
            static glm::vec2 pos = { 0, 0 };
            renderer.draw_texture(pos, { 256, 256 }, tx);
            if (window.key_state(iris::io::key::w).down()) {
                pos.y -= 1.f;
            }
            if (window.key_state(iris::io::key::s).down()) {
                pos.y += 1.f;
            }
            if (window.key_state(iris::io::key::a).down()) {
                pos.x -= 1.f;
            }
            if (window.key_state(iris::io::key::d).down()) {
                pos.x += 1.f;
            }
        }
        renderer.end_frame();
        window.poll_events();
        window.swap_buffers();
    }
}

IrisPlatformGlue
