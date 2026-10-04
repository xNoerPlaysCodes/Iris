#include "spdlog/spdlog.h"
#include <thread>
#include <utilities.hpp>

namespace iris::util {
    timer_t::timer_t(bool start) {
        if (start) this->start();
    }

    void timer_t::start() {
        start_time = clock::now();
    }

    void timer_t::stop() {
        end_time = clock::now();
        this->ended = true;
    }

    timer_t::clock::duration timer_t::elapsed() {
        if (!this->ended) return clock::now() - this->start_time;
        return this->end_time - this->start_time;
    }

    double timer_t::ms() {
        return std::chrono::duration<double, std::milli>(elapsed()).count();
    }

    double timer_t::us() {
        return std::chrono::duration<double, std::micro>(elapsed()).count();
    }

    double timer_t::sec() {
        return std::chrono::duration<double>(elapsed()).count();
    }

    double timer_t::min() {
        return std::chrono::duration<double, std::ratio<60>>(elapsed()).count();
    }

    double timer_t::hr() {
        return std::chrono::duration<double, std::ratio<3600>>(elapsed()).count();
    }

    std::string timer_t::to_str(unit_flag_t flag) {
        std::ostringstream oss;

        double total_us = this->us();

        int days = 0, hrs = 0, mins = 0, secs = 0, ms = 0, us = 0;

        constexpr auto has_flag = [](unit_flag_t flag, unit_flag_t other) -> bool {
            return static_cast<uint8_t>(flag) & static_cast<uint8_t>(other);
        };

        if (has_flag(flag, unit_flag_t::day)) {
            days = static_cast<int>(total_us / (1000.0 * 1000 * 60 * 60 * 24));
            total_us -= days * 1000.0 * 1000 * 60 * 60 * 24;
        }

        if (has_flag(flag, unit_flag_t::hr)) {
            hrs = static_cast<int>(total_us / (1000.0 * 1000 * 60 * 60));
            total_us -= hrs * 1000.0 * 1000 * 60 * 60;
        }

        if (has_flag(flag, unit_flag_t::min)) {
            mins = static_cast<int>(total_us / (1000.0 * 1000 * 60));
            total_us -= mins * 1000.0 * 1000 * 60;
        }

        if (has_flag(flag, unit_flag_t::sec)) {
            secs = static_cast<int>(total_us / (1000.0 * 1000));
            total_us -= secs * 1000.0 * 1000;
        }

        if (has_flag(flag, unit_flag_t::ms)) {
            ms = static_cast<int>(total_us / 1000.0);
            total_us -= ms * 1000;
        }

        if (has_flag(flag, unit_flag_t::us)) {
            us = static_cast<int>(total_us);
        }

        if (days) oss << days << "days, ";
        if (hrs)  oss << hrs << "hrs, ";
        if (mins) oss << mins << "mins, ";
        if (secs) oss << secs << "secs, ";
        if (ms)   oss << ms << "ms, ";
        if (us)   oss << us << "us, ";

        std::string result = oss.str();
        if (!result.empty()) result.erase(result.size() - 2); // remove trailing ", "
        return result.empty() ? "0us" : result;
    }

    // -- AI --
    void wait_for(double target_seconds) noexcept {
        using clock = std::chrono::steady_clock;
        using duration = clock::duration;
        namespace chr = std::chrono;

        static double spin_wait = 0.002;
        static double last_overshoot = 0.;

        const auto target = chr::duration<double>(target_seconds);
        const auto start = clock::now();
        const auto deadline = start + chr::duration_cast<duration>(target);

        if (target_seconds <= 0.0)
            return;

        spin_wait = std::clamp(spin_wait + last_overshoot * 0.25, 0., 0.005);

        const auto spin_duration = chr::duration_cast<duration>(chr::duration<double>(spin_wait));

        if (clock::now() + spin_duration < deadline) {
            std::this_thread::sleep_until(deadline - spin_duration);
        }

        while (clock::now() < deadline) {}

        const auto end = clock::now();
        const double elapsed = chr::duration<double>(end - start).count();

        last_overshoot = std::clamp(elapsed - target_seconds, 0., 0.01);
    }
    // -- -- --
}
