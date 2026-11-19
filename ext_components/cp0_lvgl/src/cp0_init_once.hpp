/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <mutex>
#include <utility>

namespace cp0 {

// Alpine fork: rewritten from the throw-through-std::call_once retry
// pattern. That pattern is glibc-specific: when the initializer throws,
// glibc's pthread_once resets the once state (a cleanup handler unwinds),
// so the NEXT caller re-runs the initializer — which is how retry worked.
// musl has no such cleanup: unwinding through pthread_once leaves the
// once control in its in-progress state, and the retry machinery ends in
// std::terminate (reproduced with the upstream header + the unchanged
// test_init_once contract on aarch64 musl).
//
// The mutex + state form keeps the exact contract the test pins down:
//   - one initializer execution at a time;
//   - a false return leaves the state open, and the next caller re-runs
//     the initializer (retry);
//   - concurrent callers serialize: the winner initializes, everyone
//     else sees the completed state and returns true;
//   - a true return latches forever.
// A std::mutex held across the initializer makes nested run() calls on
// the same InitOnce deadlock, same as the call_once form did (undefined
// there too); no call site nests.
class InitOnce
{
public:
    template <typename Initializer>
    bool run(Initializer &&initializer)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) return true;
        if (!std::forward<Initializer>(initializer)()) return false;
        initialized_ = true;
        return true;
    }

private:
    std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace cp0
