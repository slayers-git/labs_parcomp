#include "perf_clock.hpp"

#include <time.h>

namespace perf_clock {
    TimePoint Clock::now () noexcept {
        struct timespec ts;
        uint64_t clock = clock_gettime (CLOCK_REALTIME, &ts);

        const uint64_t sec  = ts.tv_sec;
        const uint64_t nsec = ts.tv_nsec;

        return TimePoint (sec, nsec);
    }
}
