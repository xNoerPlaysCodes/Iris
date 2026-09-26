#include "glm/fwd.hpp"
#include "iris/graphics/rendering.hpp"
#include "iris/graphics/windowing.hpp"
#include "iris/io/io.hpp"
#include "iris/types.hpp"
#include <iris/runtime.hpp>

void iris_main(const iris_main_arguments &) {
    auto init_data = iris::init();
    iris::window window = { "Iris — Core Window Test", { 800, 600 } };

    while (window.running()) {
        if (!window.visible_surface()) {
            continue;
        }
        window.poll_events();
        window.swap_buffers();
    }
}

IrisPlatformGlue
