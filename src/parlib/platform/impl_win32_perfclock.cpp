#include "perf_clock.hpp"

#include <windows.h>

namespace perf_clock {
    /* Probably going to work poorly, but windows lol */
    TimePoint Clock::now () noexcept {
        LARGE_INTEGER li;

        QueryPerformanceFrequency (&li);
        uint64_t freq = li.QuadPart;

        QueryPerformanceCounter (&li);
        uint64_t sec  = li.QuadPart / freq;
        uint64_t nsec = (li.QuadPart * 1000000000) / freq;
        
        return TimePoint (sec, nsec);
    }
}
