#include <cassert>
#include <cstdint>
#include <cmath>
#include <iostream>

#include "perf_clock.hpp"
#include "integrator.hpp"

int main (void) {
    for (uint32_t i = 1; i <= 20; ++i) {
        perf_clock::TimePoint cur = perf_clock::Clock::now ();

        TrapezoidIntegratorMT<double> integrator_mt (i);
        const auto res = integrator_mt (
            [](double x) {
                return sin (x) * cos (x);
            },
            0, M_PI, 10E-6);
        
        perf_clock::TimePoint end = perf_clock::Clock::now ();
        perf_clock::TimeDuration d = cur - end;

        std::cout << i << " - " << (double)d.count () / 1000000 << "ms, result: " << res << '\n';
    }

    return 0;
}
