#ifndef __PERF_CLOCK_HPP__
#define __PERF_CLOCK_HPP__

#include <time.h>
#include <cstdint>

#include "perf_timepoint.hpp"
#include "perf_timeduration.hpp"

namespace perf_clock {
    namespace detail { };

    class Clock {
    private:

    public:
        static TimePoint now () noexcept;
    };
};

#endif /* #define __PERF_CLOCK_HPP__ */
