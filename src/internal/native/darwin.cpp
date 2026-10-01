#include "gl.h"
#include <cstdlib>
#include <exception>
#include <native.hpp>
#include <IOKit/hid/IOHIDManager.h>
#include <format>
#include <string>
#include <CoreGraphics/CoreGraphics.h>
#include <iris/runtime.hpp>
#include <sys/sysctl.h>
#include <vector>

#include <algorithm>
#include <cstdio>

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
    void init() noexcept {}
    void terminate() noexcept {
        std::terminate(); // bor fuck off
    }

    // I don't wanna extrapolate the keyboard_connected and mouse_connected
    // functions so i'll just leave them as copies with one change okay.

    bool keyboard_connected() noexcept {
        IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

        IOHIDManagerSetDeviceMatching(manager, nullptr);

        CFSetRef devices = IOHIDManagerCopyDevices(manager);
        if (!devices) {
            CFRelease(manager);
            return false;
        }

        bool found = false;

        CFIndex count = CFSetGetCount(devices);

        std::vector<IOHIDDeviceRef> device_list(count);
        CFSetGetValues(devices,
            const_cast<const void**>(reinterpret_cast<void**>(device_list.data())));

        for (IOHIDDeviceRef device : device_list) {
            if (IOHIDDeviceConformsTo(
                    device,
                    kHIDPage_GenericDesktop,
                    kHIDUsage_GD_Keyboard)) {
                found = true;
                break;
            }
        }

        CFRelease(devices);
        CFRelease(manager);

        return found;
    }

    bool mouse_connected() noexcept {
        IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

        IOHIDManagerSetDeviceMatching(manager, nullptr);

        CFSetRef devices = IOHIDManagerCopyDevices(manager);
        if (!devices) {
            CFRelease(manager);
            return false;
        }

        bool found = false;

        CFIndex count = CFSetGetCount(devices);

        std::vector<IOHIDDeviceRef> device_list(count);
        CFSetGetValues(devices,
            const_cast<const void**>(reinterpret_cast<void**>(device_list.data())));

        for (IOHIDDeviceRef device : device_list) {
            if (IOHIDDeviceConformsTo(
                    device,
                    kHIDPage_GenericDesktop,
                    kHIDUsage_GD_Mouse)) {
                found = true;
                break;
            }
        }

        CFRelease(devices);
        CFRelease(manager);

        return found;
    }

    bool touch_connected() noexcept {
        // Bro thinks I'm adding multitouch trackpad support :sob:
        return false;
    }

    std::string device_name() noexcept {
        // AFAIK There is no way (public APIs) to do this programatically

        std::string cmd = "system_profiler SPHardwareDataType | sed -n 's/^ *Model Name: //p'";
        FILE *pipe = popen(cmd.c_str(), "r");
        char buf[512];
        fgets(buf, 512, pipe);
        pclose(pipe);
        std::string result = buf;
        result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
        return result;
    }

    glm::ivec2 screen_resolution() noexcept {
        CGDirectDisplayID id = CGMainDisplayID();
        return { CGDisplayPixelsWide(id), CGDisplayPixelsHigh(id) };
    }

    std::string processor_name() noexcept {
        char cpu_brand[255];
        size_t len = 3;
        i32 mib[3];
        sysctlnametomib("machdep.cpu.brand_string", mib, &len);
        len = sizeof(cpu_brand);
        i32 err = sysctl(mib, 3, cpu_brand, &len, NULL, 0);

        if (err == 0) {
            return cpu_brand;
        } else {
            return "Unknown";
        } 
    }
}
