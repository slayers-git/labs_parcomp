#ifndef __PERF_TIMEDURATION_HPP__
#define __PERF_TIMEDURATION_HPP__

#include <cstdint>

namespace perf_clock {
    class TimeDuration {
    private:
        uint64_t m_duration {};

    public:
        TimeDuration () = default;
        explicit TimeDuration (uint64_t s) {
            m_duration = s;
        }

        uint64_t count () const noexcept {
            return m_duration;
        }
    };
};

#endif /* #define __PERF_TIMEDURATION_HPP__ */
