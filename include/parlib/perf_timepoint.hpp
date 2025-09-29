#ifndef __PERF_TIMEPOINT_HPP__
#define __PERF_TIMEPOINT_HPP__

#include <cstdint>
#include "perf_timeduration.hpp"

namespace perf_clock {
    class TimePoint {
    private:
        uint64_t m_sec;
        uint64_t m_nsec;

        static TimeDuration diff (const TimePoint& start, const TimePoint& end) noexcept {
            uint64_t sec, nsec;

            if ((end.m_nsec - start.m_nsec) < 0) {
                sec = end.m_sec - start.m_sec - 1;
                nsec = 1000000000 + end.m_nsec - start.m_nsec;
            } else {
                sec = end.m_sec - start.m_sec;
                nsec = end.m_nsec - start.m_nsec;
            }

            return TimeDuration (sec * 1000000000 + nsec);
        }

    public:
        TimePoint () = default;
        TimePoint (uint64_t seconds, uint64_t nanoseconds) {
            m_sec = seconds;
            m_nsec = nanoseconds;
        }

        TimeDuration time_since_epoch () const noexcept {
            return TimeDuration (m_sec * 1000000000 + m_nsec);
        }

        TimeDuration operator- (const TimePoint& other) const noexcept {
            return diff (*this, other);
        }
    };
};

#endif /* #define __PERF_TIMEPOINT_HPP__ */
