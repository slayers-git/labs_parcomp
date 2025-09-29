#ifndef __INTEGRATOR_HPP__
#define __INTEGRATOR_HPP__

#include <cassert>
#include <cstdint>
#include <thread>
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <functional>

template<typename T>
using IntegratedFunction = std::function<T(T)>;

class Integrator {};

template<typename T>
class TrapezoidIntegrator : public Integrator {
public:
    constexpr T partition (IntegratedFunction<T> func, T left, T right,
            uint32_t partitions) const noexcept {
        assert (partitions > 0);
        return (*this)(func, left, right, std::abs (left - right) / partitions);
    }

    constexpr T operator () (IntegratedFunction<T> func, T left, T right,
            T step) const noexcept {
        assert (step > T{0});

        if (left == right)
            return 0;

        T result {};
        T sign = 1;
        if (left > right) {
            std::swap (left, right);
            sign = -1;
        }
            
        for (T a = left; a < right; a += step) {
            if (a + step > right)
                step = right - a;

            const T fa = func (a);
            const T fb = func (a + step);

            result += (fa + fb) * step;
        }

        return sign * result / 2;
    }
};

template<typename T>
class TrapezoidIntegratorMT : public Integrator {
private:
    uint32_t m_threads = 0;

public:
    explicit TrapezoidIntegratorMT (uint32_t thread_count = 0) noexcept {
        if (!thread_count)
            thread_count = std::thread::hardware_concurrency ();

        m_threads = thread_count;
    }

    constexpr T partition (IntegratedFunction<T> func, T left, T right,
            uint32_t partitions) const noexcept {
        assert (partitions > 0);

        const T thread_step = (right - left) / partitions;
        return (*this)(func, left, right, thread_step);
    }

    constexpr T operator () (IntegratedFunction<T> func, T left, T right,
            T step) const noexcept {
        assert (step > T{0});

        std::vector<T> results (m_threads);
        std::vector<std::thread> thread_pool (m_threads);

        const T thread_step = (right - left) / m_threads;

        for (uint32_t i = 0; i < m_threads; ++i) {
            T thread_left  = left + thread_step * i;
            T thread_right = thread_left + thread_step;

            thread_pool[i] = std::thread ([=, &results]() {
                TrapezoidIntegrator<T> integrator;
                results[i] = integrator (func, thread_left, thread_right, step);
            });
        }

        std::for_each (thread_pool.begin (), thread_pool.end (),
            [](std::thread& t) {
                t.join ();
            });

        return std::accumulate (results.begin (), results.end (), T{0});
    }
};

#endif /* #define __INTEGRATOR_HPP__ */
