#pragma once

#include <chrono>

template <typename C = std::chrono::steady_clock>
class timer {
  private:
    using time_point = typename C::time_point;

  private:
    time_point _start{C::now()};
    time_point _end{};

  public:
    void tick() {
        _end = time_point{};
        _start = C::now();
    }


    void tock() { _end = C::now(); }


    template <typename D = std::chrono::milliseconds>
    auto duration() const {
        return std::chrono::duration_cast<D>(_end - _start);
    }
};