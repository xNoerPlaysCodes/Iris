#include "glm/fwd.hpp"
#include "iris/graphics/rendering.hpp"
#include "iris/graphics/windowing.hpp"
#include "iris/io/io.hpp"
#include "iris/types.hpp"
#include <iris/runtime.hpp>

void iris_main(const iris_main_arguments &) {
    auto init_data = iris::init();
    iris::window window = { "Iris — Core Window Test", { 800, 600 } };
    iris::renderer renderer(window);

    while (window.running()) {
        if (!window.visible_surface()) {
            continue;
        }
        renderer.begin_frame();
        renderer.clear();
        {
            static glm::vec2 pos = { 0, 0 };
            renderer.draw_rectangle(pos, { 40, 40 }, { 255, 255, 0, 255 });
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

            auto touches = window.touch_points();

            if (touches.size() > 0) {
                for (auto &touch : touches) {
                    renderer.draw_rectangle(touch.second, { 100, 100 }, { 255, 0, 0, 255 });
                }
            }
        }
        renderer.end_frame();
        window.poll_events();
        window.swap_buffers();
    }
}

IrisPlatformGlue
